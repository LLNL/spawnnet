SPAWNDIR=../install

all: clean
	gcc -g -O0 -o comm comm.c -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o barrier barrier.c -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o allreduce allreduce.c -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o bcast bcast.c -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o allgather allgather.c -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2
	gcc -g -O0 -o shmallgather shmallgather.c -I$(SPAWNDIR)/include -L$(SPAWNDIR)/lib -Wl,-rpath,$(SPAWNDIR)/lib -lspawn -lpmi2 -lrt


clean:
	rm -f *.o comm barrier allreduce bcast allgather shmallgather
