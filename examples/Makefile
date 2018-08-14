# change setting here to point to spawnnet install
SPAWNDIR=../install
PMI_INCDIR=/usr/global/tools/pmi4pmix/blueos_3_ppc64le_ib/build
PMI_LIBDIR=/usr/global/tools/pmi4pmix/blueos_3_ppc64le_ib/lib

all: clean
	gcc -g -O0 -o create       create.c       comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o barrier      barrier.c      comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o allreduce    allreduce.c    comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o bcast        bcast.c        comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o allgather    allgather.c    comm.c -I. -I/usr/include/slurm  -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o shmallgather shmallgather.c comm.c -I. -I/usr/include/slurm -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2 -lrt

clean:
	rm -f *.o comm barrier allreduce bcast allgather shmallgather
