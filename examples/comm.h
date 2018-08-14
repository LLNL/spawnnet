/* defines structure and functions to build lwgrp two-level communicator */

typedef struct lwgrp_comm_t {
  lwgrp* world;   /* group of all procs in job */
  lwgrp* node;    /* group of procs on the same node (same hostname) */
  lwgrp* leaders; /* group of procs across nodes having same rank in node group */
} lwgrp_comm;

/* given an endpoint, and rank and size from PMI,
 * create a communicator object
 *   rank -  IN rank value returned from PMI2_Init
 *   size -  IN size value returned from PMI2_Init
 *   ep   -  IN endpoint created by call to spawn_net_open
 *   comm - OUT pointer to comm structure */
void comm_create(int rank, int size, spawn_net_endpoint* ep, lwgrp_comm* comm);

/* frees given comm object */
void comm_free(lwgrp_comm* comm);
