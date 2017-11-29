/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
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
#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>
#include <types_ext.h>
#include <bget_alloc.h>

void kfree(void *ptr);

#ifdef ENABLE_MDBG
/* XXX: Fix MDBG */
void *mdbg_malloc(const char *fname, int lineno, size_t size);
void *mdbg_calloc(const char *fname, int lineno, size_t nmemb, size_t size);
void *mdbg_realloc(const char *fname, int lineno, void *ptr, size_t size);
void *mdbg_memalign(const char *fname, int lineno, size_t alignment,
		size_t size);

void mdbg_check(int bufdump);

#define malloc(size)	mdbg_malloc(__FILE__, __LINE__, (size))
#define calloc(nmemb, size) \
		mdbg_calloc(__FILE__, __LINE__, (nmemb), (size))
#define realloc(ptr, size) \
		mdbg_realloc(__FILE__, __LINE__, (ptr), (size))
#define memalign(alignment, size) \
		mdbg_memalign(__FILE__, __LINE__, (alignment), (size))

#else

void *kmalloc(size_t size);
void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);
void *kmemalign(size_t alignment, size_t size);

#endif

/*
 * Adds a pool of memory to allocate from.
 */
void kmalloc_add_pool(void *buf, size_t len);

#ifdef CFG_WITH_STATS

void kmalloc_get_stats(struct malloc_stats *stats);
void kmalloc_reset_stats(void);

#endif /* CFG_WITH_STATS */

#endif /* KMALLOC_H */
