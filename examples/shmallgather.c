#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <slurm/pmi2.h>
#include <sys/time.h>

// shared mem
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

#include "spawn.h"

// use shm_open to create a shared memory segment of specified size,
// return pointer or NULL if failed
void* shmmalloc(const char* name, size_t size)
{
    // open file for shared memory
    int fd = shm_open(name, O_CREAT | O_RDWR, S_IRWXU);
    if (fd == -1) {
        printf("ERROR: shm_open(%s) %d=%s\n",
            name, errno, strerror(errno)
        );
        fflush(stdout);
        return NULL;
    }

    // set file to correct size
    if (ftruncate(fd, size) == -1) {
        printf("ERROR: ftruncate(%s, %llu) %d=%s\n",
            name, (long long unsigned) size, errno, strerror(errno)
        );
        fflush(stdout);
        close(fd);
        shm_unlink(name);
        return NULL;
    }

    // map file as shared memory
    void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (ptr == MAP_FAILED) {
        printf("ERROR: mmap(%s, %llu) %d=%s\n",
            name, (long long unsigned) size, errno, strerror(errno)
        );
        fflush(stdout);
        close(fd);
        shm_unlink(name);
        return NULL;
    }

    // done with this file descriptor
    close(fd);

    // got the file mapped, so it's safe to unlink it
    shm_unlink(name);

    return ptr;
}

// free shared memory segment allocated with shmmalloc
void shmfree(void* ptr, size_t size)
{
    munmap(ptr, size);
}

// execute ring exchange with PMIX_Ring or PMI2 calls
//   rank      - IN  process rank as returned by PMI2_Init
//   size      - IN  group size as returned by PMI2_Init
//   val       - IN  value to send to other ranks
//   ring_rank - OUT rank within the ring
//   ring_size - OUT size of ring
//   left      - OUT value from process whose ring_rank is one less than ours
//   right     - OUT value from process whose ring_rank is one more than ours
//   len       - IN  max length of any value across all processes
void ring(int rank, int size, const char* val, int* ring_rank, int* ring_size, char* left, char* right, size_t len)
{
    /* use PMIX_Ring for ring exchange */
    PMIX_Ring(val, ring_rank, ring_size, left, right, len);

    /* ring exchange the harder way with PMI2_Put/Fence/Get calls */
#if 0
    /* put our value */
    char key[128];
    sprintf(key, "ring%d", rank);
    PMI2_KVS_Put(key, val);

    /* exchange data */
    PMI2_KVS_Fence();

    /* compute left rank */
    int rank_left = rank - 1;
    if (rank_left < 0) {
        rank_left += size;
    }

    /* compute right rank */
    int rank_right = rank + 1;
    if (rank_right >= size) {
        rank_right -= size;
    }

    /* define keys for left and right ranks */
    char key_left[128];
    char key_right[128];
    sprintf(key_left,  "ring%d", rank_left);
    sprintf(key_right, "ring%d", rank_right);

    /* lookup values for left and right ranks */
    int outlen;
    PMI2_KVS_Get(NULL, PMI2_ID_NULL, key_left,  left,  len, &outlen);
    PMI2_KVS_Get(NULL, PMI2_ID_NULL, key_right, right, len, &outlen);
    //printf("%d: left=%s <-- me=%s --> right=%s\n", rank, left, val, right);
#endif
}

/* executes an allgather of address values */
void allgather(int rank, int size, int len, char* addr, char* buf)
{
    /* start timer */
    struct timeval tv;
    gettimeofday(&tv, NULL);

    /* open and endpoint and get its name */
    //spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_IBUD);
    spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_TCP);
    const char* ep_name = spawn_net_name(ep);

    /* exchange endpoint address on ring */
    int ring_rank, ring_size;
    char val[128];
    char left[128];
    char right[128];
    snprintf(val, sizeof(val), "%s", ep_name);
    ring(rank, size, val, &ring_rank, &ring_size, left, right, 128);

    /* create global comm, using left and right endpoints */
    lwgrp* comm_world = lwgrp_create(ring_size, ring_rank, ep_name, left, right, ep);

    /* get comm of procs on same node */
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    lwgrp* comm_intra = lwgrp_split_str(comm_world, hostname);

    /* get comm of leaders (procs having same rank in node communicator) */
    int rank_world = lwgrp_rank(comm_world);
    int rank_intra = lwgrp_rank(comm_intra);
    lwgrp* comm_inter = lwgrp_split(comm_world, rank_intra, rank_world);

    /* create a map and insert our address */
    strmap* map = strmap_new();
    strmap_setf(map, "%d=%s", rank, addr);

    /* allgather strmap, 
     * gather to leader, allgatherv across leaders, bcast from leader */
    lwgrp_allgather_strmap(map, comm_intra);
    if (rank_intra == 0) {
        lwgrp_allgather_strmap(map, comm_inter);
    }
    lwgrp_allgather_strmap(map, comm_intra);

    /* extract MPI address for each process and copy to buffer */
    int source_rank;
    strmap_node* node = strmap_node_first(map);
    while (node) {
        const char* key   = strmap_node_key(node);
        const char* value = strmap_node_value(node);
        sscanf(key, "%d", &source_rank);
        char* ptr = buf + source_rank * len;
        strncpy(ptr, value, len);
        node = strmap_node_next(node);
    }

    /* free the map */
    strmap_delete(&map);

    /* free communicators */
    lwgrp_free(&comm_inter);
    lwgrp_free(&comm_intra);
    lwgrp_free(&comm_world);

    /* close our endpoint and channel */
    spawn_net_close(&ep);

    /* stop timer and report cost */
    struct timeval tv2;
    gettimeofday(&tv2, NULL);
    if (rank == 0) {
        printf("%f ms\n",
               ((tv2.tv_sec - tv.tv_sec) * 1000.0
                + (tv2.tv_usec - tv.tv_usec) / 1000.0));
    }

    return;
}

/* executes an allgather of addr values and writes them to the specified shared memory buffer */
void shmallgather(int rank, int size, int len, char* addr, char* buf)
{
    /* open an endpoint and get its name */
    //spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_IBUD);
    spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_TCP);
    const char* ep_name = spawn_net_name(ep);

    /* execute ring exchange to get endpoint names of neighbors */
    int ring_rank, ring_size;
    char val[128];
    char left[128];
    char right[128];
    snprintf(val, sizeof(val), "%s", ep_name);
    ring(rank, size, val, &ring_rank, &ring_size, left, right, 128);

    /* build global comm from left and right endpoints */
    lwgrp* comm_world = lwgrp_create(ring_size, ring_rank, ep_name, left, right, ep);

    /* get comm of procs on same node */
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    lwgrp* comm_intra = lwgrp_split_str(comm_world, hostname);

    /* get comm of leaders (procs having same rank in node communicator) */
    int rank_world = lwgrp_rank(comm_world);
    int rank_intra = lwgrp_rank(comm_intra);
    lwgrp* comm_inter = lwgrp_split(comm_world, rank_intra, rank_world);

    /* create a map and insert our address
     * use our global rank as the key */
    strmap* map = strmap_new();
    strmap_setf(map, "%d=%s", rank, addr);

    /* gather addresses to leaders */
    lwgrp_allgather_strmap(map, comm_intra);

    /* leaders exchange data and fill in shared memory segment */
    if (rank_intra == 0) {
        /* gather full set of addresses to leader on each node */
        lwgrp_allgather_strmap(map, comm_inter);

        /* extract MPI address for each process and copy to shared memory */
        int source_rank;
        strmap_node* node = strmap_node_first(map);
        while (node) {
            const char* key   = strmap_node_key(node);
            const char* value = strmap_node_value(node);
            sscanf(key, "%d", &source_rank);
            char* ptr = buf + source_rank * len;
            strncpy(ptr, value, len);
            node = strmap_node_next(node);
        }
    }

    /* wait for our leader to signal that address table is complete */
    lwgrp_barrier(comm_intra);

    /* free the map */
    strmap_delete(&map);

    /* free communicators */
    lwgrp_free(&comm_inter);
    lwgrp_free(&comm_intra);
    lwgrp_free(&comm_world);

    /* close our endpoint */
    spawn_net_close(&ep);

    return;
}

int main(int argc, char **argv)
{
    /* initialize PMI, get our rank and process group size */
    int spawned, size, rank, appnum;
    PMI2_Init(&spawned, &size, &rank, &appnum);

    /* ensure all procs have complete init before starting timer */
    PMI2_KVS_Fence();

    /* start timer */
    struct timeval tv, tv2;
    gettimeofday(&tv, NULL);

    /* encode address into same length string on all procs */
    char addr[128];
    sprintf(addr, "rank%10d", rank);
    int len = (int)strlen(addr) + 1;

#if 0
    /* allocate memory region on each process and fill it with address values */
    char* addrs = (char*) malloc(size * len);
    allgather(rank, size, len, addr, addrs);
    free(addrs);
#endif

    /* allocate shared memory region and fill it with address values */
    size_t bufsize = size * len;
    char* shmaddrs = (char*) shmmalloc("/addrs", bufsize);
    shmallgather(rank, size, len, addr, shmaddrs);
    shmfree(shmaddrs, bufsize);

    /* ensure all procs have finished before stopping timer */
    PMI2_KVS_Fence();

    /* stop timer and report cost */
    gettimeofday(&tv2, NULL);
    if (rank == 0) {
        printf("%f ms shmallgather\n",
               ((tv2.tv_sec - tv.tv_sec) * 1000.0
                + (tv2.tv_usec - tv.tv_usec) / 1000.0));
    }

    /* shut down PMI */
    PMI2_Finalize();

    return 0;
}
