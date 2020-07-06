# CFLAGS=-g -pg -ansi -std=gnu99 -O2
CFLAGS=-ansi -std=gnu99 -O2
CC=gcc
OBJ=cmd-metrics
  
all:		$(OBJ)

cmd-metrics:	cmd-metrics.o inode_stats.o mempool.o
		$(CC) $(CFLAGS) -l procps -o cmd-metrics cmd-metrics.o inode_stats.o mempool.o

cmd-metrics.o:	cmd-metrics.c cmd-metrics.h
		$(CC) $(CFLAGS) -c cmd-metrics.c

inode_stats.o:	inode_stats.c inode_stats.h
		$(CC) $(CFLAGS) -c inode_stats.c

mempool.o:	mempool.c mempool.h
		$(CC) $(CFLAGS) -c mempool.c
clean:
		rm -f *.o $(OBJ) gmon.out gprof.out
