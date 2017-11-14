/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 * Copyright (c) 2017, EPAM Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>
#include <types_ext.h>

#ifdef CFG_WITH_STATS
#define TEE_ALLOCATOR_DESC_LENGTH 32
struct bget_malloc_stats {
	char desc[TEE_ALLOCATOR_DESC_LENGTH];
	uint32_t allocated;               /* Bytes currently allocated */
	uint32_t max_allocated;           /* Tracks max value of allocated */
	uint32_t size;                    /* Total size for this allocator */
	uint32_t num_alloc_fail;          /* Number of failed alloc requests */
	uint32_t biggest_alloc_fail;      /* Size of biggest failed alloc */
	uint32_t biggest_alloc_fail_used; /* Alloc bytes when above occurred */
};
#endif

struct bget_alloc_pool {
	void *buf;
	size_t len;
};

/* This is very hacky. But it will be okay for PoC */
struct bget_poolset {
	struct pbufinfo {
		long x1;
		long x2;
	} bufinfo;
	struct bget_poolset *ptr1;
	struct bget_poolset *ptr2;
	struct bget_alloc_pool *alloc_pool;
	size_t pool_len;
#ifdef CFG_WITH_STATS
	struct bget_malloc_stats mstats;
#endif
	unsigned int lock;
};

#define DEF_BGET_POOLSET(name)				\
	struct bget_bpoolset name = {{0, 0}, &name, &name, NULL, 0, 0}

void bget_free(void *ptr, struct bget_poolset *poolset);

void *bget_malloc(size_t size, struct bget_poolset *poolset);
void *bget_calloc(size_t nmemb, size_t size, struct bget_poolset *poolset);
void *bget_realloc(void *ptr, size_t size, struct bget_poolset *poolset);
void *bget_memalign(size_t alignment, size_t size,
		    struct bget_poolset *poolset);

/*
 * Returns true if the supplied memory area is within a buffer
 * previously allocated (and not freed yet).
 *
 * Used internally by TAs
 */
bool bget_malloc_buffer_is_within_alloced(void *buf, size_t len,
					  struct bget_poolset *poolset);

/*
 * Returns true if the supplied memory area is overlapping the area used
 * for heap.
 *
 * Used internally by TAs
 */
bool bget_malloc_buffer_overlaps_heap(void *buf, size_t len,
				      struct bget_poolset *poolset);

/*
 * Adds a pool of memory to allocate from.
 */
void bget_malloc_add_pool(void *buf, size_t len,
			  struct bget_poolset *poolset);

#ifdef CFG_WITH_STATS
/*
 * Get/reset allocation statistics
 */

void bget_malloc_get_stats(struct bget_malloc_stats *stats,
			   struct bget_poolset *poolset);
void bget_malloc_reset_stats(struct bget_poolset *poolset);
#endif /* CFG_WITH_STATS */

#endif /* ALLOC_H */
