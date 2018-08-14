#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <slurm/pmi2.h>

#include "spawn.h"
#include "comm.h"

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
