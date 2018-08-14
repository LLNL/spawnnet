#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <slurm/pmi2.h>
#include "pmi2.h"

#include "spawn.h"
#include "comm.h"

/* executes an allgather of address values */
void allgather(int rank, int size, int len, char* addr, char* buf, lwgrp_comm* comm)
{
    /* create a map and insert our address */
    strmap* map = strmap_new();
    strmap_setf(map, "%d=%s", rank, addr);

    /* allgather strmap, 
     * gather to leader, allgatherv across leaders, bcast from leader */
    lwgrp_allgather_strmap(map, comm->node);
    int64_t rank_node = lwgrp_rank(comm->node);
    if (rank_node == 0) {
        lwgrp_allgather_strmap(map, comm->leaders);
    }
    lwgrp_allgather_strmap(map, comm->node);

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

    /* allocate memory region on each process and fill it with address values */
    char* addrs = (char*) malloc(size * len);
    allgather(rank, size, len, addr, addrs, &comm);
    free(addrs);

    /* free communicator */
    comm_free(&comm);

    /* close our endpoint and channel */
    spawn_net_close(&ep);

    /* shut down PMI */
    PMI2_Finalize();

    return 0;
}
