#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <slurm/pmi2.h>
#include <sys/time.h>

#include "spawn.h"

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
#ifdef HAVE_PMIX_RING
    /* use PMIX_Ring for ring exchange */
    PMIX_Ring(val, ring_rank, ring_size, left, right, len);
#else
    /* ring exchange the harder way with PMI2_Put/Fence/Get calls */

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
#endif
}

typedef struct lwgrp_comm_t {
  lwgrp* world;
  lwgrp* node;
  lwgrp* leaders;
} lwgrp_comm;

/* executes an allgather of address values */
void comm_create(int rank, int size, spawn_net_endpoint* ep, lwgrp_comm* comm)
{
    /* get name of our endpoint */
    const char* ep_name = spawn_net_name(ep);

    /* exchange endpoint address on ring */
    int ring_rank, ring_size;
    char val[128];
    char left[128];
    char right[128];
    snprintf(val, sizeof(val), "%s", ep_name);
    ring(rank, size, val, &ring_rank, &ring_size, left, right, 128);

    /* create global comm, using left and right endpoints */
    comm->world = lwgrp_create(ring_size, ring_rank, ep_name, left, right, ep);

    /* get comm of procs on same node */
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    comm->node = lwgrp_split_str(comm->world, hostname);

    /* get comm of leaders (procs having same rank in node communicator) */
    int rank_world = lwgrp_rank(comm->world);
    int rank_intra = lwgrp_rank(comm->node);
    comm->leaders = lwgrp_split(comm->world, rank_intra, rank_world);

    return;
}

void comm_free(lwgrp_comm* comm)
{
    /* free communicators */
    lwgrp_free(&comm->leaders);
    lwgrp_free(&comm->node);
    lwgrp_free(&comm->world);

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

    /* open and endpoint and get its name */
    //spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_IBUD);
    spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_TCP);

    /* allocate communicator */
    lwgrp_comm comm;
    comm_create(rank, size, ep, &comm);

    /**********************
     * barrier across all processes
     **********************/
    lwgrp_barrier(comm.world);

    /**********************
     * barrier between procs on the same node
     **********************/
    lwgrp_barrier(comm.node);

    /**********************
     * barrier across all processes (two-level version),
     * procs on node signal their leader, barrier across leaders, leader signal procs on its node
     **********************/
    lwgrp_barrier(comm.node);
    uint64_t rank_node = lwgrp_rank(comm.node);
    if (rank_node == 0) {
        lwgrp_barrier(comm.leaders);
    }
    lwgrp_barrier(comm.node);

    /* free communicator */
    comm_free(&comm);

    /* close our endpoint and channel */
    spawn_net_close(&ep);

    /* ensure all procs have finished before stopping timer */
    PMI2_KVS_Fence();

    /* stop timer and report cost */
    gettimeofday(&tv2, NULL);
    if (rank == 0) {
        printf("%f ms\n",
               ((tv2.tv_sec - tv.tv_sec) * 1000.0
                + (tv2.tv_usec - tv.tv_usec) / 1000.0));
    }

    /* shut down PMI */
    PMI2_Finalize();

    return 0;
}
