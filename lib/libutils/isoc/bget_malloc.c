// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 */

#define PROTOTYPES

/*
 *  BGET CONFIGURATION
 *  ==================
 */
/* #define BGET_ENABLE_ALL_OPTIONS */
#ifdef BGET_ENABLE_OPTION
#define TestProg    20000	/* Generate built-in test program
				   if defined.  The value specifies
				   how many buffer allocation attempts
				   the test program should make. */
#endif


#ifdef __LP64__
#define SizeQuant   16
#endif
#ifdef __ILP32__
#define SizeQuant   8
#endif
				/* Buffer allocation size quantum:
				   all buffers allocated are a
				   multiple of this size.  This
				   MUST be a power of two. */

#ifdef BGET_ENABLE_OPTION
#define BufDump     1		/* Define this symbol to enable the
				   bpoold() function which dumps the
				   buffers in a buffer pool. */

#define BufValid    1		/* Define this symbol to enable the
				   bpoolv() function for validating
				   a buffer pool. */

#define DumpData    1		/* Define this symbol to enable the
				   bufdump() function which allows
				   dumping the contents of an allocated
				   or free buffer. */

#define BufStats    1		/* Define this symbol to enable the
				   bstats() function which calculates
				   the total free space in the buffer
				   pool, the largest available
				   buffer, and the total space
				   currently allocated. */

#define FreeWipe    1		/* Wipe free buffers to a guaranteed
				   pattern of garbage to trip up
				   miscreants who attempt to use
				   pointers into released buffers. */

#define BestFit     1		/* Use a best fit algorithm when
				   searching for space for an
				   allocation request.  This uses
				   memory more efficiently, but
				   allocation will be much slower. */

#define BECtl       1		/* Define this symbol to enable the
				   bectl() function for automatic
				   pool space control.  */
#endif

#ifdef MEM_DEBUG
#undef NDEBUG
#define DumpData    1
#define BufValid    1
#define FreeWipe    1
#endif

#ifdef CFG_WITH_STATS
#define BufStats    1
#endif

#include <compiler.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <trace.h>
#include <util.h>

#if defined(__KERNEL__)
/* Compiling for TEE Core */
#include <kernel/asan.h>
#include <kernel/thread.h>
#include <kernel/spinlock.h>

static void tag_asan_free(void *buf, size_t len)
{
	asan_tag_heap_free(buf, (uint8_t *)buf + len);
}

static void tag_asan_alloced(void *buf, size_t len)
{
	asan_tag_access(buf, (uint8_t *)buf + len);
}

static void *memset_unchecked(void *s, int c, size_t n)
{
	return asan_memset_unchecked(s, c, n);
}

#else /*__KERNEL__*/
/* Compiling for TA */

static void tag_asan_free(void *buf __unused, size_t len __unused)
{
}

static void tag_asan_alloced(void *buf __unused, size_t len __unused)
{
}

static void *memset_unchecked(void *s, int c, size_t n)
{
	return memset(s, c, n);
}

#endif /*__KERNEL__*/

#include "bget.c"		/* this is ugly, but this is bget */

struct malloc_pool {
	void *buf;
	size_t len;
};

struct malloc_ctx {
	struct bpoolset poolset;
	struct malloc_pool *pool;
	size_t pool_len;
#ifdef BufStats
	struct malloc_stats mstats;
#endif
#ifdef __KERNEL__
	unsigned int spinlock;
#endif
};

#ifdef __KERNEL__

static uint32_t malloc_lock(struct malloc_ctx *ctx)
{
	return cpu_spin_lock_xsave(&ctx->spinlock);
}

static void malloc_unlock(struct malloc_ctx *ctx, uint32_t exceptions)
{
	cpu_spin_unlock_xrestore(&ctx->spinlock, exceptions);
}

#else  /* __KERNEL__ */

static uint32_t malloc_lock(struct malloc_ctx *ctx __unused)
{
	return 0;
}

static void malloc_unlock(struct malloc_ctx *ctx __unused,
			  uint32_t exceptions __unused)
{
}

#endif	/* __KERNEL__ */

#define DEFINE_CTX(name) struct malloc_ctx name =		\
	{ .poolset = { .freelist = { {0, 0},			\
			{&name.poolset.freelist,		\
			 &name.poolset.freelist}}}}

static DEFINE_CTX(malloc_ctx);

#ifdef CFG_VIRTUALIZATION
static __kdata DEFINE_CTX(kmalloc_ctx);
#endif

#ifdef BufStats

static void raw_malloc_return_hook(void *p, size_t requested_size,
				   struct malloc_ctx *ctx)
{
	if (ctx->poolset.totalloc > ctx->mstats.max_allocated)
		ctx->mstats.max_allocated = ctx->poolset.totalloc;

	if (!p) {
		ctx->mstats.num_alloc_fail++;
		if (requested_size > ctx->mstats.biggest_alloc_fail) {
			ctx->mstats.biggest_alloc_fail = requested_size;
			ctx->mstats.biggest_alloc_fail_used =
				ctx->poolset.totalloc;
		}
	}
}

static void raw_malloc_reset_stats(struct malloc_ctx *ctx)
{
	uint32_t exceptions = malloc_lock(ctx);

	ctx->mstats.max_allocated = 0;
	ctx->mstats.num_alloc_fail = 0;
	ctx->mstats.biggest_alloc_fail = 0;
	ctx->mstats.biggest_alloc_fail_used = 0;
	malloc_unlock(ctx, exceptions);
}

void malloc_reset_stats(void)
{
	raw_malloc_reset_stats(&malloc_ctx);
}

static void raw_malloc_get_stats(struct malloc_ctx *ctx,
				 struct malloc_stats *stats)
{
	uint32_t exceptions = malloc_lock(ctx);

	memcpy(stats, &ctx->mstats, sizeof(*stats));
	stats->allocated = ctx->poolset.totalloc;
	malloc_unlock(ctx, exceptions);
}

void malloc_get_stats(struct malloc_stats *stats)
{
	raw_malloc_get_stats(&malloc_ctx, stats);
}

#else /* BufStats */

static void raw_malloc_return_hook(void *p __unused,
				   size_t requested_size __unused,
				   struct malloc_ctx *ctx __unused)
{
}

#endif /* BufStats */

#ifdef BufValid
static void raw_malloc_validate_pools(struct malloc_ctx *ctx)
{
	size_t n;

	for (n = 0; n < ctx->pool_len; n++)
		bpoolv(ctx->pool[n].buf);
}
#else
static void raw_malloc_validate_pools(struct malloc_ctx *ctx __unused)
{
}
#endif

struct bpool_iterator {
	struct bfhead *next_buf;
	size_t pool_idx;
};

static void bpool_foreach_iterator_init(struct malloc_ctx *ctx,
					struct bpool_iterator *iterator)
{
	iterator->pool_idx = 0;
	iterator->next_buf = BFH(ctx->pool[0].buf);
}

static bool bpool_foreach_pool(struct bpool_iterator *iterator, void **buf,
		size_t *len, bool *isfree)
{
	struct bfhead *b = iterator->next_buf;
	bufsize bs = b->bh.bsize;

	if (bs == ESent)
		return false;

	if (bs < 0) {
		/* Allocated buffer */
		bs = -bs;

		*isfree = false;
	} else {
		/* Free Buffer */
		*isfree = true;

		/* Assert that the free list links are intact */
		assert(b->ql.blink->ql.flink == b);
		assert(b->ql.flink->ql.blink == b);
	}

	*buf = (uint8_t *)b + sizeof(struct bhead);
	*len = bs - sizeof(struct bhead);

	iterator->next_buf = BFH((uint8_t *)b + bs);
	return true;
}

static bool bpool_foreach(struct malloc_ctx *ctx,
			  struct bpool_iterator *iterator, void **buf)
{
	while (true) {
		size_t len;
		bool isfree;

		if (bpool_foreach_pool(iterator, buf, &len, &isfree)) {
			if (isfree)
				continue;
			return true;
		}

		if ((iterator->pool_idx + 1) >= ctx->pool_len)
			return false;

		iterator->pool_idx++;
		iterator->next_buf = BFH(ctx->pool[iterator->pool_idx].buf);
	}
}

/* Convenience macro for looping over all allocated buffers */
#define BPOOL_FOREACH(ctx, iterator, bp)       		      \
	for (bpool_foreach_iterator_init((ctx),(iterator));   \
	     bpool_foreach((ctx),(iterator), (bp));)

static void *raw_malloc(size_t hdr_size, size_t ftr_size, size_t pl_size,
			struct malloc_ctx *ctx)
{
	void *ptr = NULL;
	bufsize s;

	/*
	 * Make sure that malloc has correct alignment of returned buffers.
	 * The assumption is that uintptr_t will be as wide as the largest
	 * required alignment of any type.
	 */
	COMPILE_TIME_ASSERT(SizeQuant >= sizeof(uintptr_t));

	raw_malloc_validate_pools(ctx);

	/* Compute total size */
	if (ADD_OVERFLOW(pl_size, hdr_size, &s))
		goto out;
	if (ADD_OVERFLOW(s, ftr_size, &s))
		goto out;

	/* BGET doesn't like 0 sized allocations */
	if (!s)
		s++;

	ptr = bget(s, &ctx->poolset);
out:
	raw_malloc_return_hook(ptr, pl_size, ctx);

	return ptr;
}

static void raw_free(void *ptr, struct malloc_ctx *ctx)
{
	raw_malloc_validate_pools(ctx);

	if (ptr)
		brel(ptr, &ctx->poolset);
}

static void *raw_calloc(size_t hdr_size, size_t ftr_size, size_t pl_nmemb,
			size_t pl_size, struct malloc_ctx *ctx)
{
	void *ptr = NULL;
	bufsize s;

	raw_malloc_validate_pools(ctx);

	/* Compute total size */
	if (MUL_OVERFLOW(pl_nmemb, pl_size, &s))
		goto out;
	if (ADD_OVERFLOW(s, hdr_size, &s))
		goto out;
	if (ADD_OVERFLOW(s, ftr_size, &s))
		goto out;

	/* BGET doesn't like 0 sized allocations */
	if (!s)
		s++;

	ptr = bgetz(s, &ctx->poolset);
out:
	raw_malloc_return_hook(ptr, pl_nmemb * pl_size, ctx);

	return ptr;
}

static void *raw_realloc(void *ptr, size_t hdr_size, size_t ftr_size,
			 size_t pl_size, struct malloc_ctx *ctx)
{
	void *p = NULL;
	bufsize s;

	/* Compute total size */
	if (ADD_OVERFLOW(pl_size, hdr_size, &s))
		goto out;
	if (ADD_OVERFLOW(s, ftr_size, &s))
		goto out;

	raw_malloc_validate_pools(ctx);

	/* BGET doesn't like 0 sized allocations */
	if (!s)
		s++;

	p = bgetr(ptr, s, &ctx->poolset);
out:
	raw_malloc_return_hook(p, pl_size, ctx);

	return p;
}

static void create_free_block(struct bfhead *bf, bufsize size, struct bhead *bn,
			      struct bpoolset *poolset)
{
	assert(BH((char *)bf + size) == bn);
	assert(bn->bsize < 0); /* Next block should be allocated */
	/* Next block shouldn't already have free block in front */
	assert(bn->prevfree == 0);

	/* Create the free buf header */
	bf->bh.bsize = size;
	bf->bh.prevfree = 0;

	/* Update next block to point to the new free buf header */
	bn->prevfree = size;

	/* Insert the free buffer on the free list */
	assert(poolset->freelist.ql.blink->ql.flink == &poolset->freelist);
	assert(poolset->freelist.ql.flink->ql.blink == &poolset->freelist);
	bf->ql.flink = &poolset->freelist;
	bf->ql.blink = poolset->freelist.ql.blink;
	poolset->freelist.ql.blink = bf;
	bf->ql.blink->ql.flink = bf;
}

static void brel_before(char *orig_buf, char *new_buf, struct bpoolset *poolset)
{
	struct bfhead *bf;
	struct bhead *b;
	bufsize size;
	bufsize orig_size;

	assert(orig_buf < new_buf);
	/* There has to be room for the freebuf header */
	size = (bufsize)(new_buf - orig_buf);
	assert(size >= (SizeQ + sizeof(struct bhead)));

	/* Point to head of original buffer */
	bf = BFH(orig_buf - sizeof(struct bhead));
	orig_size = -bf->bh.bsize; /* negative since it's an allocated buffer */

	/* Point to head of the becoming new allocated buffer */
	b = BH(new_buf - sizeof(struct bhead));

	if (bf->bh.prevfree != 0) {
		/* Previous buffer is free, consolidate with that buffer */
		struct bfhead *bfp;

		/* Update the previous free buffer */
		bfp = BFH((char *)bf - bf->bh.prevfree);
		assert(bfp->bh.bsize == bf->bh.prevfree);
		bfp->bh.bsize += size;

		/* Make a new allocated buffer header */
		b->prevfree = bfp->bh.bsize;
		/* Make it negative since it's an allocated buffer */
		b->bsize = -(orig_size - size);
	} else {
		/*
		 * Previous buffer is allocated, create a new buffer and
		 * insert on the free list.
		 */

		/* Make it negative since it's an allocated buffer */
		b->bsize = -(orig_size - size);

		create_free_block(bf, size, b, poolset);
	}

#ifdef BufStats
	poolset->totalloc -= size;
	assert(poolset->totalloc >= 0);
#endif
}

static void brel_after(char *buf, bufsize size, struct bpoolset *poolset)
{
	struct bhead *b = BH(buf - sizeof(struct bhead));
	struct bhead *bn;
	bufsize new_size = size;
	bufsize free_size;

	/* Select the size in the same way as in bget() */
	if (new_size < SizeQ)
		new_size = SizeQ;
#ifdef SizeQuant
#if SizeQuant > 1
	new_size = (new_size + (SizeQuant - 1)) & (~(SizeQuant - 1));
#endif
#endif
	new_size += sizeof(struct bhead);
	assert(new_size <= -b->bsize);

	/*
	 * Check if there's enough space at the end of the buffer to be
	 * able to free anything.
	 */
	free_size = -b->bsize - new_size;
	if (free_size < SizeQ + sizeof(struct bhead))
		return;

	bn = BH((char *)b - b->bsize);
	/*
	 * Set the new size of the buffer;
	 */
	b->bsize = -new_size;
	if (bn->bsize > 0) {
		/* Next buffer is free, consolidate with that buffer */
		struct bfhead *bfn = BFH(bn);
		struct bfhead *nbf = BFH((char *)b + new_size);
		struct bhead *bnn = BH((char *)bn + bn->bsize);

		assert(bfn->bh.prevfree == 0);
		assert(bnn->prevfree == bfn->bh.bsize);

		/* Construct the new free header */
		nbf->bh.prevfree = 0;
		nbf->bh.bsize = bfn->bh.bsize + free_size;

		/* Update the buffer after this to point to this header */
		bnn->prevfree += free_size;

		/*
		 * Unlink the previous free buffer and link the new free
		 * buffer.
		 */
		assert(bfn->ql.blink->ql.flink == bfn);
		assert(bfn->ql.flink->ql.blink == bfn);

		/* Assing blink and flink from old free buffer */
		nbf->ql.blink = bfn->ql.blink;
		nbf->ql.flink = bfn->ql.flink;

		/* Replace the old free buffer with the new one */
		nbf->ql.blink->ql.flink = nbf;
		nbf->ql.flink->ql.blink = nbf;
	} else {
		/* New buffer is allocated, create a new free buffer */
		create_free_block(BFH((char *)b + new_size), free_size, bn, poolset);
	}

#ifdef BufStats
	poolset->totalloc -= free_size;
	assert(poolset->totalloc >= 0);
#endif

}

static void *raw_memalign(size_t hdr_size, size_t ftr_size, size_t alignment,
			  size_t size, struct malloc_ctx *ctx)
{
	size_t s;
	uintptr_t b;

	raw_malloc_validate_pools(ctx);

	if (!IS_POWER_OF_TWO(alignment))
		return NULL;

	/*
	 * Normal malloc with headers always returns something SizeQuant
	 * aligned.
	 */
	if (alignment <= SizeQuant)
		return raw_malloc(hdr_size, ftr_size, size, ctx);

	s = hdr_size + ftr_size + alignment + size +
	    SizeQ + sizeof(struct bhead);

	/* Check wapping */
	if (s < alignment || s < size)
		return NULL;

	b = (uintptr_t)bget(s, &ctx->poolset);
	if (!b)
		goto out;

	if ((b + hdr_size) & (alignment - 1)) {
		/*
		 * Returned buffer is not aligned as requested if the
		 * hdr_size is added. Find an offset into the buffer
		 * that is far enough in to the buffer to be able to free
		 * what's in front.
		 */
		uintptr_t p;

		/*
		 * Find the point where the buffer including supplied
		 * header size should start.
		 */
		p = b + hdr_size + alignment;
		p &= ~(alignment - 1);
		p -= hdr_size;
		if ((p - b) < (SizeQ + sizeof(struct bhead)))
			p += alignment;
		assert((p + hdr_size + ftr_size + size) <= (b + s));

		/* Free the front part of the buffer */
		brel_before((void *)b, (void *)p, &ctx->poolset);

		/* Set the new start of the buffer */
		b = p;
	}

	/*
	 * Since b is now aligned, release what we don't need at the end of
	 * the buffer.
	 */
	brel_after((void *)b, hdr_size + ftr_size + size, &ctx->poolset);
out:
	raw_malloc_return_hook((void *)b, size, ctx);

	return (void *)b;
}

/* Most of the stuff in this function is copied from bgetr() in bget.c */
static __maybe_unused bufsize bget_buf_size(void *buf)
{
	bufsize osize;          /* Old size of buffer */
	struct bhead *b;

	b = BH(((char *)buf) - sizeof(struct bhead));
	osize = -b->bsize;
#ifdef BECtl
	if (osize == 0) {
		/*  Buffer acquired directly through acqfcn. */
		struct bdhead *bd;

		bd = BDH(((char *)buf) - sizeof(struct bdhead));
		osize = bd->tsize - sizeof(struct bdhead);
	} else
#endif
		osize -= sizeof(struct bhead);
	assert(osize > 0);
	return osize;
}

#ifdef ENABLE_MDBG

struct mdbg_hdr {
	const char *fname;
	uint16_t line;
	uint32_t pl_size;
	uint32_t magic;
#if defined(ARM64)
	uint64_t pad;
#endif
};

#define MDBG_HEADER_MAGIC	0xadadadad
#define MDBG_FOOTER_MAGIC	0xecececec

static size_t mdbg_get_ftr_size(size_t pl_size)
{
	size_t ftr_pad = ROUNDUP(pl_size, sizeof(uint32_t)) - pl_size;

	return ftr_pad + sizeof(uint32_t);
}

static uint32_t *mdbg_get_footer(struct mdbg_hdr *hdr)
{
	uint32_t *footer;

	footer = (uint32_t *)((uint8_t *)(hdr + 1) + hdr->pl_size +
			      mdbg_get_ftr_size(hdr->pl_size));
	footer--;
	return footer;
}

static void mdbg_update_hdr(struct mdbg_hdr *hdr, const char *fname,
		int lineno, size_t pl_size)
{
	uint32_t *footer;

	hdr->fname = fname;
	hdr->line = lineno;
	hdr->pl_size = pl_size;
	hdr->magic = MDBG_HEADER_MAGIC;

	footer = mdbg_get_footer(hdr);
	*footer = MDBG_FOOTER_MAGIC;
}

static void *raw_mdbg_malloc(struct malloc_ctx *ctx, const char *fname,
			     int lineno, size_t size)
{
	struct mdbg_hdr *hdr;
	uint32_t exceptions = malloc_lock(ctx);

	/*
	 * Check struct mdbg_hdr doesn't get bad alignment.
	 * This is required by C standard: the buffer returned from
	 * malloc() should be aligned with a fundamental alignment.
	 * For ARM32, the required alignment is 8. For ARM64, it is 16.
	 */
	COMPILE_TIME_ASSERT(
		(sizeof(struct mdbg_hdr) % (__alignof(uintptr_t) * 2)) == 0);

	hdr = raw_malloc(sizeof(struct mdbg_hdr),
			 mdbg_get_ftr_size(size), size, ctx);
	if (hdr) {
		mdbg_update_hdr(hdr, fname, lineno, size);
		hdr++;
	}

	malloc_unlock(ctx, exceptions);
	return hdr;
}

static void assert_header(struct mdbg_hdr *hdr __maybe_unused)
{
	assert(hdr->magic == MDBG_HEADER_MAGIC);
	assert(*mdbg_get_footer(hdr) == MDBG_FOOTER_MAGIC);
}

static void raw_mdbg_free(struct malloc_ctx *ctx, void *ptr)
{
	struct mdbg_hdr *hdr = ptr;

	if (hdr) {
		hdr--;
		assert_header(hdr);
		hdr->magic = 0;
		*mdbg_get_footer(hdr) = 0;
		raw_free(hdr, ctx);
	}
}

void free(void *ptr)
{
	uint32_t exceptions = malloc_lock(&malloc_ctx);

	raw_mdbg_free(&malloc_ctx, ptr);
	malloc_unlock(&malloc_ctx, exceptions);
}

static void *raw_mdbg_calloc(struct malloc_ctx *ctx, const char *fname, int lineno,
		      size_t nmemb, size_t size)
{
	struct mdbg_hdr *hdr;
	uint32_t exceptions = malloc_lock(ctx);

	hdr = raw_calloc(sizeof(struct mdbg_hdr),
			  mdbg_get_ftr_size(nmemb * size), nmemb, size,
			  ctx);
	if (hdr) {
		mdbg_update_hdr(hdr, fname, lineno, nmemb * size);
		hdr++;
	}
	malloc_unlock(ctx, exceptions);
	return hdr;
}

static void *raw_mdbg_realloc_unlocked(struct malloc_ctx *ctx, const char *fname,
				       int lineno, void *ptr, size_t size)
{
	struct mdbg_hdr *hdr = ptr;

	if (hdr) {
		hdr--;
		assert_header(hdr);
	}
	hdr = raw_realloc(hdr, sizeof(struct mdbg_hdr),
			   mdbg_get_ftr_size(size), size, ctx);
	if (hdr) {
		mdbg_update_hdr(hdr, fname, lineno, size);
		hdr++;
	}
	return hdr;
}

static void *raw_mdbg_realloc(struct malloc_ctx *ctx, const char *fname,
			      int lineno, void *ptr, size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(ctx);

	p = raw_mdbg_realloc_unlocked(ctx, fname, lineno, ptr, size);
	malloc_unlock(ctx, exceptions);
	return p;
}

#define realloc_unlocked(ctx, ptr, size)					\
		raw_mdbg_realloc_unlocked(ctx, __FILE__, __LINE__, (ptr), (size))

static void *raw_mdbg_memalign(struct malloc_ctx *ctx, const char *fname,
			       int lineno, size_t alignment, size_t size)
{
	struct mdbg_hdr *hdr;
	uint32_t exceptions = malloc_lock(ctx);

	hdr = raw_memalign(sizeof(struct mdbg_hdr), mdbg_get_ftr_size(size),
			   alignment, size, ctx);
	if (hdr) {
		mdbg_update_hdr(hdr, fname, lineno, size);
		hdr++;
	}
	malloc_unlock(ctx, exceptions);
	return hdr;
}


static void *get_payload_start_size(void *raw_buf, size_t *size)
{
	struct mdbg_hdr *hdr = raw_buf;

	assert(bget_buf_size(hdr) >= hdr->pl_size);
	*size = hdr->pl_size;
	return hdr + 1;
}

static void raw_mdbg_check(struct malloc_ctx *ctx, int bufdump)
{
	struct bpool_iterator itr;
	void *b;
	uint32_t exceptions = malloc_lock(ctx);

	raw_malloc_validate_pools(ctx);

	BPOOL_FOREACH(ctx, &itr, &b) {
		struct mdbg_hdr *hdr = (struct mdbg_hdr *)b;

		assert_header(hdr);

		if (bufdump > 0) {
			const char *fname = hdr->fname;

			if (!fname)
				fname = "unknown";

			IMSG("buffer: %d bytes %s:%d\n",
				hdr->pl_size, fname, hdr->line);
		}
	}

	malloc_unlock(ctx, exceptions);
}

void *mdbg_malloc(const char *fname, int lineno, size_t size)
{
	return raw_mdbg_malloc(&malloc_ctx, fname, lineno, size);
}

void *mdbg_calloc(const char *fname, int lineno, size_t nmemb, size_t size)
{
	return raw_mdbg_calloc(&malloc_ctx, fname, lineno, nmemb, size);
}

void *mdbg_realloc(const char *fname, int lineno, void *ptr, size_t size)
{
	return raw_mdbg_realloc(&malloc_ctx, fname, lineno, ptr, size);
}

void *mdbg_memalign(const char *fname, int lineno, size_t alignment,
		    size_t size)
{
	return raw_mdbg_memalign(&malloc_ctx, fname, lineno, alignment, size);
}

void mdbg_check(int bufdump)
{
	raw_mdbg_check(&malloc_ctx, bufdump);
}
#else

void *malloc(size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&malloc_ctx);

	p = raw_malloc(0, 0, size, &malloc_ctx);
	malloc_unlock(&malloc_ctx, exceptions);
	return p;
}

void free(void *ptr)
{
	uint32_t exceptions = malloc_lock(&malloc_ctx);

	raw_free(ptr, &malloc_ctx);
	malloc_unlock(&malloc_ctx, exceptions);
}

void *calloc(size_t nmemb, size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&malloc_ctx);

	p = raw_calloc(0, 0, nmemb, size, &malloc_ctx);
	malloc_unlock(&malloc_ctx, exceptions);
	return p;
}

static void *realloc_unlocked(struct malloc_ctx *ctx, void *ptr,
			      size_t size)
{
	return raw_realloc(ptr, 0, 0, size, ctx);
}

void *realloc(void *ptr, size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&malloc_ctx);

	p = realloc_unlocked(&malloc_ctx, ptr, size);
	malloc_unlock(&malloc_ctx, exceptions);
	return p;
}

void *memalign(size_t alignment, size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&malloc_ctx);

	p = raw_memalign(0, 0, alignment, size, &malloc_ctx);
	malloc_unlock(&malloc_ctx, exceptions);
	return p;
}

static void *get_payload_start_size(void *ptr, size_t *size)
{
	*size = bget_buf_size(ptr);
	return ptr;
}

#endif

static void raw_malloc_add_pool(struct malloc_ctx *ctx, void *buf, size_t len)
{
	void *p;
	size_t l;
	uint32_t exceptions;
	uintptr_t start = (uintptr_t)buf;
	uintptr_t end = start + len;
	const size_t min_len = ((sizeof(struct malloc_pool) + (SizeQuant - 1)) &
					(~(SizeQuant - 1))) +
				sizeof(struct bhead) * 2;


	start = ROUNDUP(start, SizeQuant);
	end = ROUNDDOWN(end, SizeQuant);
	assert(start < end);

	if ((end - start) < min_len) {
		DMSG("Skipping too small pool");
		return;
	}

	exceptions = malloc_lock(ctx);

	tag_asan_free((void *)start, end - start);
	bpool((void *)start, end - start, &ctx->poolset);
	l = ctx->pool_len + 1;
	p = realloc_unlocked(ctx, ctx->pool, sizeof(struct malloc_pool) * l);
	assert(p);
	ctx->pool = p;
	ctx->pool[ctx->pool_len].buf = (void *)start;
	ctx->pool[ctx->pool_len].len = end - start;
#ifdef BufStats
	ctx->mstats.size += ctx->pool[ctx->pool_len].len;
#endif
	ctx->pool_len = l;
	malloc_unlock(ctx, exceptions);
}

static bool raw_malloc_buffer_is_within_alloced(struct malloc_ctx *ctx,
						void *buf, size_t len)
{
	struct bpool_iterator itr;
	void *b;
	uint8_t *start_buf = buf;
	uint8_t *end_buf = start_buf + len;
	bool ret = false;
	uint32_t exceptions = malloc_lock(ctx);

	raw_malloc_validate_pools(ctx);

	/* Check for wrapping */
	if (start_buf > end_buf)
		goto out;

	BPOOL_FOREACH(ctx, &itr, &b) {
		uint8_t *start_b;
		uint8_t *end_b;
		size_t s;

		start_b = get_payload_start_size(b, &s);
		end_b = start_b + s;

		if (start_buf >= start_b && end_buf <= end_b) {
			ret = true;
			goto out;
		}
	}

out:
	malloc_unlock(ctx, exceptions);

	return ret;
}

static bool raw_malloc_buffer_overlaps_heap(struct malloc_ctx *ctx,
					    void *buf, size_t len)
{
	uintptr_t buf_start = (uintptr_t) buf;
	uintptr_t buf_end = buf_start + len;
	size_t n;
	bool ret = false;
	uint32_t exceptions = malloc_lock(ctx);

	raw_malloc_validate_pools(ctx);

	for (n = 0; n < ctx->pool_len; n++) {
		uintptr_t pool_start = (uintptr_t)ctx->pool[n].buf;
		uintptr_t pool_end = pool_start + ctx->pool[n].len;

		if (buf_start > buf_end || pool_start > pool_end) {
			ret = true;	/* Wrapping buffers, shouldn't happen */
			goto out;
		}

		if (buf_end > pool_start || buf_start < pool_end) {
			ret = true;
			goto out;
		}
	}

out:
	malloc_unlock(ctx, exceptions);
	return ret;
}

void malloc_add_pool(void *buf, size_t len)
{
	raw_malloc_add_pool(&malloc_ctx, buf, len);
}

bool malloc_buffer_is_within_alloced(void *buf, size_t len)
{
	return raw_malloc_buffer_is_within_alloced(&malloc_ctx, buf, len);
}

bool malloc_buffer_overlaps_heap(void *buf, size_t len)
{
	return raw_malloc_buffer_overlaps_heap(&malloc_ctx, buf, len);
}

#ifdef CFG_VIRTUALIZATION

void *kmalloc(size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&kmalloc_ctx);

	p = raw_malloc(0, 0, size, &kmalloc_ctx);
	malloc_unlock(&kmalloc_ctx, exceptions);
	return p;
}

void kfree(void *ptr)
{
	uint32_t exceptions = malloc_lock(&kmalloc_ctx);

	raw_free(ptr, &kmalloc_ctx);
	malloc_unlock(&kmalloc_ctx, exceptions);
}

void *kcalloc(size_t nmemb, size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&kmalloc_ctx);

	p = raw_calloc(0, 0, nmemb, size, &kmalloc_ctx);
	malloc_unlock(&kmalloc_ctx, exceptions);
	return p;
}

void *krealloc(void *ptr, size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&kmalloc_ctx);

	p = realloc_unlocked(&kmalloc_ctx, ptr, size);
	malloc_unlock(&kmalloc_ctx, exceptions);
	return p;
}

void *kmemalign(size_t alignment, size_t size)
{
	void *p;
	uint32_t exceptions = malloc_lock(&kmalloc_ctx);

	p = raw_memalign(0, 0, alignment, size, &kmalloc_ctx);
	malloc_unlock(&kmalloc_ctx, exceptions);
	return p;
}

void kmalloc_add_pool(void *buf, size_t len)
{
	raw_malloc_add_pool(&kmalloc_ctx, buf, len);
}

bool kmalloc_buffer_is_within_alloced(void *buf, size_t len)
{
	return raw_malloc_buffer_is_within_alloced(&kmalloc_ctx, buf, len);
}

bool kmalloc_buffer_overlaps_heap(void *buf, size_t len)
{
	return raw_malloc_buffer_overlaps_heap(&kmalloc_ctx, buf, len);
}

void kmalloc_reset_stats(void)
{
	raw_malloc_reset_stats(&kmalloc_ctx);
}

void kmalloc_get_stats(struct malloc_stats *stats)
{
	raw_malloc_get_stats(&kmalloc_ctx, stats);
}

#endif
