#include <bget_alloc.h>
#include <kernel/kmalloc.h>

struct bget_poolset kmalloc_poolset __kdata =
	BGET_INIT_POOLSET(kmalloc_poolset);

void kfree(void *ptr)
{
	bget_free(ptr, &kmalloc_poolset);
}

void *kmalloc(size_t size)
{
	return bget_malloc(size, &kmalloc_poolset);
}

void *kcalloc(size_t nmemb, size_t size)
{
	return bget_calloc(nmemb, size, &kmalloc_poolset);
}

void *krealloc(void *ptr, size_t size)
{
	return bget_realloc(ptr, size, &kmalloc_poolset);
}

void *kmemalign(size_t alignment, size_t size)
{
	return bget_memalign(alignment, size, &kmalloc_poolset);
}

void kmalloc_add_pool(void *buf, size_t len)
{
	bget_malloc_add_pool(buf, len, &kmalloc_poolset);
}

void kmalloc_get_stats(struct malloc_stats *stats)
{
	bget_malloc_get_stats(stats, &kmalloc_poolset);
}

void kmalloc_reset_stats(void)
{
	bget_malloc_reset_stats(&kmalloc_poolset);
}
