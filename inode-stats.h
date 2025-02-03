// Struct voor de socket-nodes in de hash-table
struct sock_ino_ent {
        struct sock_ino_ent *next;
        unsigned int    ino;
        unsigned int    uid;
        unsigned int    addr_loc;
        unsigned int    port_loc;
        unsigned int    addr_rem;
        unsigned int    port_rem;
        unsigned int    state;
};

struct sock_aggr {
        unsigned long sock_total;
        struct state {
	        unsigned long established;
	        unsigned long close_wait;
        	unsigned long listener;
//      	unsigned long time_wait;  // TIME_WAIT heeft geen zin, want zulke sockets hebben geen owner/process
        	unsigned long rest;
	} state;
};

typedef struct sock_ino_ent sock_ino_ent_t;
typedef struct sock_aggr    sock_aggr_t;

#define INO_HASH_SIZE 256
#define INITIAL_READBUF_SIZE (1024*1024)
#define INO_PROCESS_LEN_MAX 16

void sock_ino_build_hash_table(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL **pool_ino, char **read_buf, long *buflen);
void sock_ino_destroy_hash_table(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL *pool_ino);
void sock_ino_print(sock_ino_ent_t *p, int pid);
void sock_aggr_print(sock_aggr_t *s);
sock_aggr_t * sock_ino_gather_cmd_stats(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL **pool_ino, char *cmd);
