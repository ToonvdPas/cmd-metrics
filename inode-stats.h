/*
 * Copyright (C) 2025 Toon van der Pas, Houten.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
#define INO_NAME_LEN_MAX 1024

void sock_ino_build_hash_table(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL **pool_ino, char **read_buf, long *buflen);
void sock_ino_destroy_hash_table(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL *pool_ino);
void sock_ino_print(sock_ino_ent_t *p, int pid);
void sock_aggr_print(sock_aggr_t *s);
sock_aggr_t * sock_ino_gather_cmd_stats(sock_ino_ent_t *hash_array[INO_HASH_SIZE], POOL **pool_ino, char *cmd);
