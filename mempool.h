typedef struct pool {
	size_t size;
	char *next;
	char *end;
} POOL;

POOL * pool_create(size_t size);
POOL * pool_extend(POOL *p);
void   pool_destroy(POOL *p);
void   pool_reset(POOL *p);
size_t pool_size(POOL *p);
size_t pool_available(POOL *p);
void * pool_alloc(POOL *p, size_t size);
