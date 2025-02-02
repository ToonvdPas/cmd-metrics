# CFLAGS=-g -pg -ansi -std=gnu99 -O2
CFLAGS=-ansi -std=gnu99 -O2
CC=gcc
OBJ=cmd-metrics
  
all:		$(OBJ)

cmd-metrics:	cmd-metrics.o inode-stats.o mempool.o
		$(CC) $(CFLAGS) -l proc2 -o cmd-metrics cmd-metrics.o inode-stats.o mempool.o

cmd-metrics.o:	cmd-metrics.c cmd-metrics.h procps-pids.h mempool.h
		$(CC) $(CFLAGS) -c cmd-metrics.c

inode-stats.o:	inode-stats.c inode-stats.h
		$(CC) $(CFLAGS) -c inode-stats.c

mempool.o:	mempool.c mempool.h
		$(CC) $(CFLAGS) -c mempool.c
clean:
		rm -f *.o $(OBJ) gmon.out gprof.out
