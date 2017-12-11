#ifndef __KERNEL_VIRT_MAPPER_H
#define __KERNEL_VIRT_MAPPER_H

#include <kernel/virtualization.h>

struct mapper_ctx;

void init_virt_mapper(struct tee_mmap_region *memory_map);
int virt_mapper_map_client(struct client_context *ctx);
int virt_mapper_unmap_client(struct client_context *ctx);
int virt_mapper_add_client(struct client_context *ctx);
void virt_mapper_remove_client(struct client_context *ctx);
void *virt_mapper_alloc_table(void);
void virt_mapper_get_ta_range(struct client_context *ctx,
			      paddr_t *start, paddr_t *end);
#endif
