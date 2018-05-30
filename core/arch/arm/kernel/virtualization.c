// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2018, EPAM Systems. All rights reserved. */

#include <compiler.h>
#include <platform_config.h>
#include <kernel/generic_boot.h>
#include <kernel/linker.h>
#include <kernel/mutex.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <kernel/spinlock.h>
#include <kernel/virtualization.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <mm/tee_mm.h>
#include <platform_config.h>
#include <sm/optee_smc.h>
#include <string.h>
#include <util.h>

static unsigned int ctx_list_lock __kdata = SPINLOCK_UNLOCK;

static LIST_HEAD(ctx_list_head, guest_context) ctx_list __kdata =
	LIST_HEAD_INITIALIZER(ctx_list_head);

/* Free pages used for guest contexts */
tee_mm_pool_t virt_mapper_pool __kbss;

/* Memory used by OP-TEE core */
struct tee_mmap_region *kmemory_map __kbss;

struct guest_context
{
	uint16_t id;
	void *tables_va;
	tee_mm_entry_t *tee_ram;
	tee_mm_entry_t *ta_ram;
	tee_mm_entry_t *tables;
	struct mmu_context *mmu_ctx;
	struct tee_mmap_region *memory_map;
	unsigned int spinlock;
	LIST_ENTRY(guest_context) next;
	bool runtime_initialized;
	uint16_t refcnt;
	struct mutex mutex;
};

struct guest_context *current_context[CFG_TEE_CORE_NB_CORE] __kbss;

static struct guest_context* get_current_ctx(void)
{
	struct guest_context *ret;
	uint32_t exceptions = thread_mask_exceptions(THREAD_EXCP_FOREIGN_INTR);

	ret = current_context[get_core_pos()];

	thread_unmask_exceptions(exceptions);

	return ret;
}

static void set_current_ctx(struct guest_context *ctx)
{
	uint32_t exceptions = thread_mask_exceptions(THREAD_EXCP_FOREIGN_INTR);

	current_context[get_core_pos()] = ctx;

	thread_unmask_exceptions(exceptions);
}

static struct tee_mmap_region *prepare_memory_map(paddr_t tee_data, paddr_t ta_ram)
{
	int i, entries;
	vaddr_t max_va = 0;
	struct tee_mmap_region *map;
	/*
	 * This function assumes that at time of operation,
	 * kmemory_map (aka static_memory_map from core_mmu.c)
	 * will not be altered. This is true, because all
	 * changes to static_memory_map are done during
	 * OP-TEE initialization, while this function will
	 * called when hypervisor creates a guest.
	 */

	/* Count number of entries in kernel memory map */
	for (map = kmemory_map, entries = 1; map->type != MEM_AREA_END;
	     map++, entries++);

	/* Allocate entries for virtual guest map */
	map = kcalloc(entries + 1, sizeof(struct tee_mmap_region));
	if (!map)
		return NULL;

	memcpy(map, kmemory_map, sizeof(*map) * entries);

	/* Map TEE .data and .bss sections */
	for (i = 0; i < entries; i++) {
		if (map[i].va == (vaddr_t)(VCORE_UNPG_RW_PA)) {
			map[i].type = MEM_AREA_TEE_RAM_RW;
			map[i].attr = core_mmu_type_to_attr(map[i].type);
			map[i].pa = tee_data;
		}
		if (map[i].va + map[i].size > max_va)
			max_va = map[i].va + map[i].size;
	}

	/* Map TA_RAM */
	assert(map[entries - 1].type == MEM_AREA_END);
	map[entries] = map[entries - 1];
	map[entries - 1].region_size = SMALL_PAGE_SIZE;
	map[entries - 1].va = ROUNDUP(max_va, map[entries - 1].region_size);
	map[entries - 1].va += (ta_ram - map[entries - 1].va) & CORE_MMU_PGDIR_MASK;
	map[entries - 1].pa = ta_ram;
	map[entries - 1].size = TA_RAM_SIZE;
	map[entries - 1].type = MEM_AREA_TA_RAM;
	map[entries - 1].attr = core_mmu_type_to_attr(map[entries - 1].type);

	DMSG("New map (%08lx):\n",  (vaddr_t)(VCORE_UNPG_RW_PA));

	for (i = 0; i < entries; i++)
		DMSG("T: %-16s rsz: %08x, pa: %08lx, va: %08lx, sz: %08lx attr: %x\n",
		     teecore_memtype_name(map[i].type),
		     map[i].region_size, map[i].pa, map[i].va,
		     map[i].size, map[i].attr);
	return map;
}

void virt_init_memory(struct tee_mmap_region *memory_map)
{
	struct tee_mmap_region *map;

	/* Init page pool that covers all secure RAM */
	if (!tee_mm_init(&virt_mapper_pool, TEE_RAM_START,
			 TA_RAM_START + TA_RAM_SIZE,
			 SMALL_PAGE_SHIFT,
			 TEE_MM_POOL_KMALLOC))
		panic("Can't create pool with free pages");
	DMSG("Created virtual mapper pool from %x to %x\n",
	     TEE_RAM_START, TA_RAM_START + TA_RAM_SIZE);

	/* Carve out areas that are used by OP-TEE core */
	for (map = memory_map; map->type != MEM_AREA_END; map++) {
		switch(map->type) {
		case MEM_AREA_TEE_RAM_RX:
		case MEM_AREA_TEE_RAM_RO:
		case MEM_AREA_KERN_RAM_RW:
			DMSG("Carving out area of type %d (0x%08lx-0x%08lx)",
			     map->type, map->pa, map->pa + map->size);
			if (!tee_mm_alloc2(&virt_mapper_pool, map->pa,
					   map->size))
				panic("Can't carve out used area");
			break;
		default:
			continue;
		}
	}

	kmemory_map = memory_map;
}


static int configure_guest_ctx_mem(struct guest_context *ctx)
{
	int ret;
	paddr_t original_data_pa;

	ctx->tee_ram = tee_mm_alloc(&virt_mapper_pool, VCORE_UNPG_RW_SZ);
	if (!ctx->tee_ram) {
		EMSG("Can't allocate memory for TEE runtime context");
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}
	DMSG("TEE RAM: %08" PRIxPA "\n", tee_mm_get_smem(ctx->tee_ram));

	ctx->ta_ram = tee_mm_alloc(&virt_mapper_pool, TA_RAM_SIZE / VIRT_GUEST_COUNT);
	if (!ctx->ta_ram) {
		EMSG("Can't allocate memory for TA data");
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}
	DMSG("TA RAM: %08" PRIxPA "\n", tee_mm_get_smem(ctx->ta_ram));

	ctx->tables = tee_mm_alloc(&virt_mapper_pool,
				   core_mmu_get_total_pages_size());
	if (!ctx->tables) {
		EMSG("Can't allocate memory for page tables");
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}

	ctx->tables_va = phys_to_virt(tee_mm_get_smem(ctx->tables),
				      MEM_AREA_SEC_RAM_OVERALL);
	assert(ctx->tables_va);

	ctx->mmu_ctx = core_alloc_mmu_ctx(ctx->tables_va);
	if (!ctx->mmu_ctx) {
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}

	ctx->memory_map = prepare_memory_map(tee_mm_get_smem(ctx->tee_ram),
					     tee_mm_get_smem(ctx->ta_ram));
	if (!ctx->memory_map) {
		ret = TEE_ERROR_OUT_OF_MEMORY;
		goto err;
	}

	core_init_mmu_ctx(ctx->mmu_ctx, ctx->memory_map);

	original_data_pa = virt_to_phys(__data_start);
	/* Switch to guest's mappings */
	core_mmu_set_ctx(ctx->mmu_ctx);

	/* clear .bss */
	memset((void*)(VCORE_UNPG_RW_PA), 0, VCORE_UNPG_RW_SZ);

	/* copy .data section from R/O original */
	memcpy(__data_start,
	       phys_to_virt(original_data_pa, MEM_AREA_SEC_RAM_OVERALL),
	       __data_end - __data_start);

	return 0;

err:
	if (ctx->tee_ram)
		tee_mm_free(ctx->tee_ram);
	if (ctx->ta_ram)
		tee_mm_free(ctx->ta_ram);
	if (ctx->tables)
		tee_mm_free(ctx->tables);
	kfree(ctx->mmu_ctx);
	kfree(ctx->memory_map);

	return ret;
}

uint32_t virt_guest_created(uint16_t guest_id)
{
	struct guest_context *ctx;
	uint32_t exceptions;

	ctx = kmalloc(sizeof(*ctx));
	if (!ctx)
		return OPTEE_SMC_RETURN_ENOTAVAIL;

	memset(ctx, 0, sizeof(*ctx));

	ctx->id = guest_id;
	mutex_init(&ctx->mutex);
	if (configure_guest_ctx_mem(ctx)) {
		kfree(ctx);
		return OPTEE_SMC_RETURN_ENOTAVAIL;
	}

	set_current_ctx(ctx);

	/* Initialize threads */
	thread_init_threads();

	exceptions = cpu_spin_lock_xsave(&ctx_list_lock);
	LIST_INSERT_HEAD(&ctx_list, ctx, next);
	cpu_spin_unlock_xrestore(&ctx_list_lock, exceptions);

	IMSG("Added guest %d", guest_id);

	set_current_ctx(NULL);
	core_mmu_set_default_ctx();
	return OPTEE_SMC_RETURN_OK;
}

uint32_t virt_guest_destroyed(uint16_t guest_id)
{
	struct guest_context *ctx;
	uint32_t exceptions;

	IMSG("Removing guest %d", guest_id);

	if (guest_id == HYP_CLNT_ID)
		return OPTEE_SMC_RETURN_OK;

	exceptions = cpu_spin_lock_xsave(&ctx_list_lock);

	LIST_FOREACH(ctx, &ctx_list, next) {
		if (ctx->id == guest_id) {
			LIST_REMOVE(ctx, next);
			break;
		}
	}
	cpu_spin_unlock_xrestore(&ctx_list_lock, exceptions);

	if (ctx) {
		cpu_spin_lock(&ctx->spinlock);
		if (ctx->refcnt != 0) {
			EMSG("Guest thread(s) is still running. refcnt = %d\n",
			     ctx->refcnt);
			panic();
		}
		cpu_spin_unlock(&ctx->spinlock);

		tee_mm_free(ctx->tee_ram);
		tee_mm_free(ctx->ta_ram);
		tee_mm_free(ctx->tables);
		core_free_mmu_ctx(ctx->mmu_ctx);
		kfree(ctx->memory_map);
		kfree(ctx);
	} else
		EMSG("Client with id %d is not found", guest_id);

	return OPTEE_SMC_RETURN_OK;
}

bool virt_set_guest(uint16_t guest_id)
{
	struct guest_context *ctx;
	uint32_t exceptions;

	if (guest_id == HYP_CLNT_ID)
		return true;

	ctx = get_current_ctx();

	/* This can be true only if we return from IRQ RPC */
	/* TODO: Reconsider this */
	if (ctx && ctx->id == guest_id)
		return true;

	if (ctx)
		panic("Virtual guest context is already set");

	exceptions = cpu_spin_lock_xsave(&ctx_list_lock);
	LIST_FOREACH(ctx, &ctx_list, next) {
		if (ctx->id == guest_id) {
			set_current_ctx(ctx);
			core_mmu_set_ctx(ctx->mmu_ctx);
			cpu_spin_lock(&ctx->spinlock);
			ctx->refcnt++;
			cpu_spin_unlock(&ctx->spinlock);
			cpu_spin_unlock_xrestore(&ctx_list_lock,
						 exceptions);
			return true;
		}
	}
	cpu_spin_unlock_xrestore(&ctx_list_lock, exceptions);

	return false;
}

void virt_unset_guest(void)
{
	struct guest_context *ctx = get_current_ctx();

	if (!ctx)
		return;

	set_current_ctx(NULL);
	core_mmu_set_default_ctx();
	assert(ctx->refcnt);
	cpu_spin_lock(&ctx->spinlock);
	ctx->refcnt--;

	cpu_spin_unlock(&ctx->spinlock);
}

void virt_on_stdcall(void)
{
	struct guest_context *ctx = get_current_ctx();

	/* Initialize runtime on first std call */
	if (!ctx->runtime_initialized) {
		mutex_lock(&ctx->mutex);
		if (!ctx->runtime_initialized) {
			init_tee_runtime();
			ctx->runtime_initialized = true;
		}
		mutex_unlock(&ctx->mutex);
	}
}

struct tee_mmap_region *virt_get_memory_map(void)
{
	struct guest_context *ctx;

	ctx = get_current_ctx();

	if (!ctx)
		return NULL;

	return ctx->memory_map;
}

void virt_get_ta_ram(vaddr_t *start, vaddr_t *end)
{
	struct guest_context *ctx = get_current_ctx();

	*start = (vaddr_t)phys_to_virt(tee_mm_get_smem(ctx->ta_ram), MEM_AREA_TA_RAM);
	*end = *start + tee_mm_get_bytes(ctx->ta_ram);
}
