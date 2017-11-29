#include <bget_alloc.h>
#include <malloc.h>

struct bget_poolset malloc_poolset = BGET_INIT_POOLSET(malloc_poolset);

void free(void *ptr)
{
	bget_free(ptr, &malloc_poolset);
}

void *malloc(size_t size)
{
	return bget_malloc(size, &malloc_poolset);
}

void *calloc(size_t nmemb, size_t size)
{
	return bget_calloc(nmemb, size, &malloc_poolset);
}

void *realloc(void *ptr, size_t size)
{
	return bget_realloc(ptr, size, &malloc_poolset);
}

void *memalign(size_t alignment, size_t size)
{
	return bget_memalign(alignment, size, &malloc_poolset);
}

bool malloc_buffer_is_within_alloced(void *buf, size_t len)
{
	return bget_malloc_buffer_is_within_alloced(buf, len, &malloc_poolset);
}

bool malloc_buffer_overlaps_heap(void *buf, size_t len)
{
	return bget_malloc_buffer_overlaps_heap(buf, len, &malloc_poolset);
}

void malloc_add_pool(void *buf, size_t len)
{
	bget_malloc_add_pool(buf, len, &malloc_poolset);
}

void malloc_get_stats(struct malloc_stats *stats)
{
	bget_malloc_get_stats(stats, &malloc_poolset);
}

void malloc_reset_stats(void)
{
	bget_malloc_reset_stats(&malloc_poolset);
}
