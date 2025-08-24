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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mempool.h"

POOL * pool_create(size_t size) {
	size_t remainder;

	POOL *p = (POOL *) malloc(size + sizeof(POOL));
	if(p) {
		p->size = size;
		p->next = (char *) &p[1];  // registreer de start van de memory-pool
		remainder = (size_t) ((unsigned long long) (p->next) % 4);
		if (remainder == 0)
			remainder = 4;
		p->next = p->next + 4 - remainder;  // 32-bit aligned
		p->end = p->next + size;   // registreer het einde van de memory-pool
		return p;
	} else {
		printf("ERROR - failed malloc(%ld) %d - %s\n", size + sizeof(POOL), errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void pool_destroy(POOL *p) {
	free(p);
}

void pool_reset(POOL *p) {
	size_t remainder;

	p->next = (char *) &p[1];  // reset de pointer naar de start van de memory-pool
	remainder = (size_t) ((unsigned long long) (p->next) % 4);
	if (remainder == 0)
		remainder = 4;
	p->next = p->next + 4 - remainder;  // 32-bit aligned
}

size_t pool_size(POOL *p) {
	return p->size;
}

size_t pool_available(POOL *p) {
	return (p->end) - (p->next);
}

void * pool_alloc(POOL *p, size_t size) {
	size_t remainder;

//	printf("pool_alloc(%p, %lld), available: %d", p, size, pool_available(p));
	if (pool_available(p) < size) {
//		printf(" - NO SPACE LEFT IN POOL!!!\n");
		return NULL;
	} else {
		void *mem = (void *) p->next;
//		printf(", returned address %p\n", mem);
//		printf("Adres POOL-start before alignment:  %p\n", p->next + size);  // DEBUG
		remainder = (size_t) ((unsigned long long) (p->next + size) % 4);
//		printf("Remainder: %d\n", remainder);                                // DEBUG
		if (remainder == 0)
			remainder = 4;
		p->next += size + 4 - remainder;  // 32-bit aligned
//		printf("Adres POOL-start after  alignment:  %p\n", p->next);         // DEBUG
		return mem;
	}
}

POOL * pool_extend(POOL *p) {
	POOL *p_new;
	size_t remainder;

//	printf("pool_extend() REALLOC REQUESTED!!!\n");
//	printf("pool_extend() Size of POOL struct: %d\n", sizeof(POOL));
//	printf("pool_extend() available: %d, pool size: %lld\n", pool_available(p), pool_size(p));
	char *old_p    = (char *) p;
	char *old_next = (char *) (p->next);
	size_t old_size = p->size;
	size_t new_size = (p->size)*2;
//	printf("pool_extend() old_p    before realloc to %d bytes: %p\n", new_size, old_p);
//	printf("pool_extend() old_next before realloc to %d bytes: %p\n", new_size, old_next);
//	printf("pool_extend() pool pointer p before realloc to %d bytes: %p\n", new_size, p);
//	printf("pool_extend() calling realloc(%p, %lld\n", p, new_size + sizeof(POOL));
	p_new = realloc(p, new_size + sizeof(POOL));
//	printf("pool_extend() pool pointer p after  realloc to %d bytes: %p\n", new_size, p_new);
	if (p_new) {
		// Pool-size extension geslaagd!  Pas de metadata aan.
		p_new->size = new_size;
		char *new_p    = (char *) p_new;
		char *new_next = new_p + (old_next - old_p);
		p_new->next = (char *) new_next;
		p_new->end  = (char *) p_new + new_size + sizeof(POOL);
//		printf("pool_extend() new p: %p, p->next: %p, p->end: %p, p->size: %p\n", p_new, p_new->next, p_new->end, p_new->size);
//		printf("pool_extend() available: %d, pool size: %d\n", pool_available(p_new), pool_size(p_new));
	}
	return p_new;
}

#ifdef MODULE_TEST
int main(int argc, char **argv) {
	POOL *p;

	p = pool_create(1024*1024);
	printf("Mem avail: %d\n", pool_available(p));
	char *bla  = (char *) pool_alloc(p, 10*sizeof(char));
	char *bla2 = (char *) pool_alloc(p, 12*sizeof(char));
	char *bla3 = (char *) pool_alloc(p, 13*sizeof(char));
	strcpy(bla,  "Testje");
	strcpy(bla2, "Testje2");
	strcpy(bla3, "Testje3");
	printf("De variable bla  bevat de tekst \"%s\"\n", bla);
	printf("De variable bla2 bevat de tekst \"%s\"\n", bla2);
	printf("De variable bla3 bevat de tekst \"%s\"\n", bla3);
	printf("Size  POOL-struct: 0x%x\n", sizeof(POOL));
	printf("Adres POOL-start:  %p\n", p);
	printf("Adres POOL-end:    %p\n", p->end);
	printf("Adres bla:         %p\n", bla);
	printf("Adres bla2:        %p\n", bla2);
	printf("Adres bla3:        %p\n", bla3);
	printf("Pool size: %d\n", pool_size(p));
	printf("Pool free: %d\n", pool_available(p));
	printf("Pool used: %d\n", pool_size(p) - pool_available(p));
	pool_destroy(p);
}
#endif  // MODULE_TEST
