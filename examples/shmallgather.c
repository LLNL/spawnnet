#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <slurm/pmi2.h>
#include "pmi2.h"

// shared mem
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

#include "spawn.h"
#include "comm.h"

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

/* executes an allgather of addr values and writes them to the specified shared memory buffer */
void shmallgather(int rank, int size, int len, char* addr, char* buf, lwgrp_comm* comm)
{
    /* create a map and insert our address
     * use our global rank as the key */
    strmap* map = strmap_new();
    strmap_setf(map, "%d=%s", rank, addr);

    /* gather addresses to leaders */
    lwgrp_allgather_strmap(map, comm->node);

    /* leaders exchange data and fill in shared memory segment */
    int64_t rank_node = lwgrp_rank(comm->node);
    if (rank_node == 0) {
        /* gather full set of addresses to leader on each node */
        lwgrp_allgather_strmap(map, comm->leaders);

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
    lwgrp_barrier(comm->node);

    /* free the map */
    strmap_delete(&map);

    return;
}

int main(int argc, char **argv)
{
    /* initialize PMI, get our rank and process group size */
    int spawned, size, rank, appnum;
    PMI2_Init(&spawned, &size, &rank, &appnum);

    /* open and endpoint and get its name */
    //spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_IBUD);
    spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_TCP);

    /* allocate communicator */
    lwgrp_comm comm;
    comm_create(rank, size, ep, &comm);

    /* encode address into same length string on all procs */
    char addr[128];
    sprintf(addr, "rank%10d", rank);
    int len = (int)strlen(addr) + 1;

    /* allocate shared memory region and fill it with address values */
    size_t bufsize = size * len;
    char* shmaddrs = (char*) shmmalloc("/addrs", bufsize);
    shmallgather(rank, size, len, addr, shmaddrs, &comm);
    shmfree(shmaddrs, bufsize);

    /* free communicator */
    comm_free(&comm);

    /* close our endpoint and channel */
    spawn_net_close(&ep);

    /* shut down PMI */
    PMI2_Finalize();

    return 0;
}
