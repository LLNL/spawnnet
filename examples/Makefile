# change setting here to point to spawnnet install
SPAWNDIR=../install

all: clean
	gcc -g -O0 -o create       create.c       comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o barrier      barrier.c      comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o allreduce    allreduce.c    comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o bcast        bcast.c        comm.c -I. -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o allgather    allgather.c    comm.c -I. -I/usr/include/slurm  -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o shmallgather shmallgather.c comm.c -I. -I/usr/include/slurm -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2 -lrt

clean:
	rm -f *.o comm barrier allreduce bcast allgather shmallgather
