#include <kernel/panic.h>
#include <kernel/misc.h>
#include <kernel/kmalloc.h>
#include <kernel/linker.h>
#include <kernel/spinlock.h>
#include <kernel/virtualization.h>
#include <kernel/virt_mapper.h>
#include <kernel/thread.h>
#include <mm/tee_mm.h>
#include <mm/core_mmu.h>
#include <string.h>
#include <platform_config.h>

/* TODO: Testing only. Should be calculated in runtime */
#define PGT_PER_DOMAIN	20

/* Free pages used for client contexts */
tee_mm_pool_t virt_mapper_pool __kbss;

struct mapper_ctx
{
	paddr_t ttbr0[CFG_TEE_CORE_NB_CORE];
	void *tables_va;
	tee_mm_entry_t *tee_ram;
	tee_mm_entry_t *ta_ram;
	tee_mm_entry_t *tables;
	int num_free_tables;
};

/* THIS IS A HUGE HACK */
static struct mapper_ctx *current_init_ctx __kbss;

static int map_client_initial(struct client_context *ctx)
{
	unsigned int i;
	paddr_t *pages;
	int ret;

	pages = kcalloc(VCORE_UNPG_RW_SZ / SMALL_PAGE_SIZE, sizeof(paddr_t));

	if (!pages)
		return TEE_ERROR_OUT_OF_MEMORY;

	/* Map .data section */
	for (i = 0; i < VCORE_UNPG_RW_SZ / SMALL_PAGE_SIZE; i++) {
		pages[i] = tee_mm_get_smem(ctx->mapper_ctx->tee_ram) +
			i * SMALL_PAGE_SIZE;
	}

	ret = core_mmu_map_pages((vaddr_t)(VCORE_UNPG_RW_PA), pages, i,
				 MEM_AREA_TEE_RAM_RW);

	if (ret != TEE_SUCCESS)
		goto out;

	core_mmu_set_tee_ram_pa(pages[0]);
out:
	kfree(pages);
	return ret;
}

void virt_mapper_get_ta_range(struct client_context *ctx __maybe_unused,
			      paddr_t *start, paddr_t *end)
{
	struct mapper_ctx *mapper_ctx = current_init_ctx;

	*start = tee_mm_get_smem(mapper_ctx->ta_ram);
	*end = *start + tee_mm_get_bytes(mapper_ctx->ta_ram);
}

void init_virt_mapper(struct tee_mmap_region *memory_map)
{
	struct tee_mmap_region *map;
	/* Init pool that covers all secure RAM */
	if (!tee_mm_init(&virt_mapper_pool, CFG_TEE_RAM_START,
			 CFG_TA_RAM_START + CFG_WHOLE_TA_RAM_SIZE,
			 SMALL_PAGE_SHIFT,
			 TEE_MM_POOL_KMALLOC))
		panic("Can't create pool with free pages");
	DMSG("Created virtual mapper pool from %x to %x\n",
	     CFG_TEE_RAM_START, CFG_TA_RAM_START + CFG_WHOLE_TA_RAM_SIZE);
	/* Carve out areas that should not be paged out */
	for (map = memory_map; map->type != MEM_AREA_END; map++) {
		switch(map->type) {
		case MEM_AREA_TEE_RAM_RX:
		case MEM_AREA_TEE_RAM_RO:
		case MEM_AREA_KERN_RAM_RW:
		case MEM_AREA_KERN_RAM:
			DMSG("Carving out are of type %d (0x%08lx-0x%08lx)",
			     map->type, map->pa, map->pa + map->size);
			if (!tee_mm_alloc2(&virt_mapper_pool, map->pa,
					   map->size))
				panic("Can't carve out used area");
			break;
		default:
			continue;
		}
	}
}

int virt_mapper_map_client(struct client_context *ctx)
{
	core_mmu_set_ttbr0(ctx->mapper_ctx->ttbr0[get_core_pos()]);

	return 0;
}

int virt_mapper_unmap_client(struct client_context *ctx __maybe_unused)
{
	core_mmu_set_ttbr0_default();

	return 0;
}

int virt_mapper_add_client(struct client_context *ctx)
{
	int ret;
	struct mapper_ctx *mapper_ctx;

	core_mmu_set_ttbr0_default();

	mapper_ctx = kcalloc(1, sizeof(*mapper_ctx));
	if (!mapper_ctx)
		return TEE_ERROR_OUT_OF_MEMORY;

	current_init_ctx = mapper_ctx;

	mapper_ctx->tee_ram = tee_mm_alloc(&virt_mapper_pool, VCORE_UNPG_RW_SZ);
	if (!mapper_ctx->tee_ram) {
		EMSG("Can't allocate memory for TEE runtime context");
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}

	mapper_ctx->ta_ram = tee_mm_alloc(&virt_mapper_pool, CFG_TA_RAM_SIZE);
	if (!mapper_ctx->ta_ram) {
		EMSG("Can't allocate memory for TA data");
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}

	mapper_ctx->tables = tee_mm_alloc(&virt_mapper_pool,
				       PGT_PER_DOMAIN * SMALL_PAGE_SIZE );
	if (!mapper_ctx->tables) {
		EMSG("Can't allocate memory for pagetables");
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}
	mapper_ctx->tables_va = phys_to_virt(tee_mm_get_smem(mapper_ctx->tables),
					     MEM_AREA_SEC_RAM_VASPACE);
	assert(mapper_ctx->tables_va);
	mapper_ctx->num_free_tables = PGT_PER_DOMAIN;

	core_mmu_copy_curr_mapping(mapper_ctx->tables_va,
				   &mapper_ctx->num_free_tables,
				   mapper_ctx->ttbr0);
	ctx->mapper_ctx = mapper_ctx;

	ret = virt_mapper_map_client(ctx);
	if (ret) {
		EMSG("Can't map client data");
		goto err;
	}

	map_client_initial(ctx);

	/* clear .bss */
	memset((void*)(VCORE_UNPG_RW_PA), 0, VCORE_UNPG_RW_SZ);

	/* copy .data section from safe copy */
	memcpy(__data_start, __data_copy_start,
	       __data_copy_end - __data_copy_start);

	DMSG("Domain ttbr0 = %lx\n", mapper_ctx->ttbr0[0]);

	return 0;

err:
	if (mapper_ctx->tee_ram)
		tee_mm_free(mapper_ctx->tee_ram);
	if (mapper_ctx->ta_ram)
		tee_mm_free(mapper_ctx->ta_ram);
	if (mapper_ctx->tables)
		tee_mm_free(mapper_ctx->tables);
	kfree(mapper_ctx);

	return ret;
}

void virt_mapper_remove_client(struct client_context *ctx)
{
	struct mapper_ctx *mapper_ctx;

	mapper_ctx = ctx->mapper_ctx;
	tee_mm_free(mapper_ctx->tee_ram);
	tee_mm_free(mapper_ctx->ta_ram);
	tee_mm_free(mapper_ctx->tables);
	kfree(mapper_ctx);

	ctx->mapper_ctx = NULL;
}

void *virt_mapper_alloc_table(void)
{
	struct mapper_ctx *mapper_ctx = current_init_ctx;

	if (!mapper_ctx->num_free_tables)
		return NULL;

	return (char*)mapper_ctx->tables_va +
		SMALL_PAGE_SIZE * (PGT_PER_DOMAIN - mapper_ctx->num_free_tables--);
}
