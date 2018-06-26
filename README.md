# Spawnnet overview
SpawnNet provides three related packages for fast network communication,
targeted for communication between MPI processes and the process
launcher as well as between MPI processes.

spawnnet - A simple sockets-like interface for fast, reliable
point-2-point communication.  Provides calls such as connect,
accept, read, write, and disconnect.  Designed to be ported
over low-level networking interfaces.  Current implementations
include TCP and IBUD (Infiniband Verbs Unreliable Datagram).

strmap - Data structure that maps one string to another string,
which provides for a collection of key-value pairs.  Functions
are included to serialize and transfer strmap objects between
processes using spawnnet calls.

lwgrp - Provides light-weight process group representations
along with a few collectives and fast group splitting operations.

To include headers for all packages, include "spawn.h".

To get started, see the [examples README](examples/README.md).

See the following headers for more details:
  spawn_net.h
  strmap.h
  lwgrp.h
  spawn_util.h
  spawn_net_util.h
  
To build from a clone, review and run the buildme scripts:

````
# do this once to fetch and build necessary autotools versions
./buildme_autotools

# do this each time to build
./buildme

# or alternatively
./configure \
  --prefix=`pwd`/install \
  LDFLAGS="-libverbs" \
  --disable-silent-rules
make
make install
````
