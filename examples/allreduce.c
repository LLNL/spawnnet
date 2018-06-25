#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <slurm/pmi2.h>

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

    /* open and endpoint and get its name */
    //spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_IBUD);
    spawn_net_endpoint* ep = spawn_net_open(SPAWN_NET_TYPE_TCP);

    /* allocate communicator */
    lwgrp_comm comm;
    comm_create(rank, size, ep, &comm);

    uint64_t count;

    /**********************
     * sum values of rank across all processes
     **********************/
    count = (uint64_t) rank;
    lwgrp_allreduce_uint64_sum(&count, 1, comm.world);
    // sum is in count, should be (0 + 1 + ... + size-1)
    if (rank == 0) {
        printf("sum: %d\n", (int) count);
    }

    /**********************
     * get max rank value across all processes
     **********************/
    count = (uint64_t) rank;
    lwgrp_allreduce_uint64_max(&count, 1, comm.world);
    // max is in count, should be (size-1)
    if (rank == 0) {
        printf("max: %d\n", (int) count);
    }

    /**********************
     * logical OR across all processes
     **********************/
    // assume all are false
    count = 0;
    if (rank == 0) {
        // except for some reason rank 0 has a true value
        count = 1;
    }
    // get global sum of number of true values
    lwgrp_allreduce_uint64_sum(&count, 1, comm.world);
    if (count > 0) {
        // at least one process had a true value
    } else {
        // no process had a true value
    }
    if (rank == 0) {
        printf("or: %d\n", (int) (count > 0));
    }

    /**********************
     * logical AND across all processes
     **********************/
    // assume all are true
    count = 1;
    if (rank == 0) {
        // except for some reason rank 0 is false
        count = 0;
    }
    // global sum of number of true values
    lwgrp_allreduce_uint64_sum(&count, 1, comm.world);
    if (count == size) {
        // all procs are true
    } else {
        // at least one is false
    }
    if (rank == 0) {
        printf("and: %d\n", (int) (count == size));
    }

    /* free communicator */
    comm_free(&comm);

    /* close our endpoint and channel */
    spawn_net_close(&ep);

    /* shut down PMI */
    PMI2_Finalize();

    return 0;
}
