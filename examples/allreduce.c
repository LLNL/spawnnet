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
