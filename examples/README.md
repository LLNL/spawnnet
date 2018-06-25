#Examples
This directory contains working examples of how one might use spawnnet
to exchange information during MPI startup.

The examples use PMIX_Ring if available.
PMIX_Ring is provided in MVAPICH2 and the SLURM pmi2 plugin.
If not available, PMI2 Put/Fence/Get is used as a fallback.

Edit [Makefile](Makefile} to build.

## Two-level communicators
In some cases, it is useful to create multiple process groups during MPI startup.
For example, it is common to need a world group of all processes,
a group of all processes on the same node,
and another group of processes to serve as leader representative processes across nodes.

comm - demo to create world, node, and leader groups and package them into a single structure

## Barrier, Allreduce, Bcast
In some cases, an MPI library may need to synchronize processes
and perform collectives like broadcast and allreduce.

barrier - example barriers

allreduce - execute global sum, max, logical OR, and logical AND operations

bcast - use strmap to broadcast data to all ranks

## Allgather
An MPI library may wish to gather a network adress for each process
that is used to establish a connection with a remote rank.
This is often done by executing an allgather across processes using PMI.
Here are two alternatives showing this allgather using spawnnet.

allgather - executes an allgather, such that each process ends up
with a full copy of the address string from every other process.

shmallgather - same as allgather, but one copy of the address
table is shared among procs on the same node.
