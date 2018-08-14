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

    /* use our PMI rank and size for our ring rank and size values */
    *ring_rank = rank;
    *ring_size = size;

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

/* create world, node, and leader groups and store in comm struct */
void comm_create(int rank, int size, spawn_net_endpoint* ep, lwgrp_comm* comm)
{
    /* get name of our endpoint */
    const char* ep_name = spawn_net_name(ep);

    /* exchange endpoint address on ring */
    int ring_rank, ring_size;
    char val[128], left[128], right[128];
    snprintf(val, sizeof(val), "%s", ep_name);
    ring(rank, size, val, &ring_rank, &ring_size, left, right, 128);

    /* create global comm, using left and right endpoints */
    comm->world = lwgrp_create(ring_size, ring_rank, ep_name, left, right, ep);

    /* get comm of procs on same node */
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    comm->node = lwgrp_split_str(comm->world, hostname);

    /* get comm of leaders (procs having same rank in node communicator) */
    int64_t color = lwgrp_rank(comm->node);
    int64_t key   = lwgrp_rank(comm->world);
    comm->leaders = lwgrp_split(comm->world, color, key);

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

    /**********************
     * broadcast string from rank 0 to all processes
     **********************/
    strmap* map = strmap_new();
    if (rank == 0) {
        // rank 0 is the root, only one to set any key/value pairs
        strmap_setf(map, "val=%s", "hello world");
    }
    lwgrp_allgather_strmap(map, comm.world);
    if (rank == size-1) {
        // print value from last rank
        const char* data = strmap_get(map, "val");
        printf("received: %s\n", data);
    }
    strmap_delete(&map);

    /**********************
     * same thing, but using two level
     * gather to leader, allgatherv across leaders, bcast from leader to procs on node
     * pulls data into each node once rather than once per proc per node (good if data is large)
     **********************/
    map = strmap_new();
    if (rank == 0) {
        strmap_setf(map, "val=%s", "hello world");
    }
    lwgrp_allgather_strmap(map, comm.node);
    int64_t rank_node = lwgrp_rank(comm.node);
    if (rank_node == 0) {
        lwgrp_allgather_strmap(map, comm.leaders);
    }
    lwgrp_allgather_strmap(map, comm.node);
    if (rank == size-1) {
        const char* data = strmap_get(map, "val");
        printf("received: %s\n", data);
    }
    strmap_delete(&map);

    /* free communicator */
    comm_free(&comm);

    /* close our endpoint and channel */
    spawn_net_close(&ep);

    /* shut down PMI */
    PMI2_Finalize();

    return 0;
}
