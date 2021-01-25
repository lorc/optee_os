// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, EPAM Systems
 */
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>

#include "rcar.h"
#include "romapi.h"

static int get_api_table_index(void)
{
	/*
	 * Depending on SoC type and version, there are 5 possible addresses
	 * for each ROMAPI function
	 */
	static int index __nex_data = -1;

	if (index != -1)
		return index;

	switch (rcar_get_product_type()) {
	case RCAR_H3_ES1_0:
	case RCAR_H3_ES1_1:
		index = 0;
		break;
	case RCAR_H3_ES2_0:
		index = 1;
		break;
	case RCAR_M3_ES1_00:
	case RCAR_M3_ES1_05:
		index = 2;
		break;
	case RCAR_H3_ES3_0:
	case RCAR_M3_ES1_06:
	case RCAR_M3_ES1_1:
	case RCAR_V3M_ES_2_0:
	case RCAR_D3:
	case RCAR_E3:
	case RCAR_M3N:
	case RCAR_V3H:
		index = 3;
		break;
	case RCAR_V3M_ES_1_0:
		index = 4;
		break;
	default:
		panic("Unknown RCAR product type");
	}

	return index;
}

/* implemented in romapi_call.S */
extern uint32_t __plat_call_romapi(paddr_t sp, paddr_t func, uint64_t arg1,
				   uint64_t arg2, uint64_t arg3);

static uint32_t plat_call_romapi(paddr_t func, uint64_t arg1,
				 uint64_t arg2, uint64_t arg3)
{
	/*
	 * If MMU is enabled, we need to use trampoline function that will
	 * disable MMU and switch stack pointer to physical address. On other
	 * hand, if MMU is disabled, we can call the ROM function directly.
	 */
	if (cpu_mmu_enabled()) {
		uint32_t (*fptr)(paddr_t func, uint64_t arg1, uint64_t arg2,
				 uint64_t arg3);

		/*
		 * __plat_call_romapi() is identity mapped and we should call it
		 * at identity address, otherwise it will experience problem
		 * when MMU will be disabled.
		 */
		fptr = (void *)virt_to_phys(__plat_call_romapi);

		return fptr(func, arg1, arg2, arg3);
	} else {
		uint32_t (*fptr)(uint64_t arg1, uint64_t arg2, uint64_t arg3);

		fptr = (typeof(fptr))func;

		return fptr(arg1, arg2, arg3);
	}
}

static paddr_t va2pa(void *ptr)
{
	if (cpu_mmu_enabled())
		return virt_to_phys(ptr);
	else
		return (paddr_t)ptr;
}

static const paddr_t romapi_getrndvector[] = {
	0xEB10DFC4,	/* H3 1.0/1.1, needs confirmation */
	0xEB117134,	/* H3 2.0 */
	0xEB11055C,	/* M3 1.0/1.05, needs confirmation */
	0xEB100188,	/* H3 3.0, M3 1.1+, M3N, E3, D3, V3M 2.0 */
	0xEB1103A0,	/* V3M 1.0, needs confirmation */
};

uint32_t plat_rom_getrndvector(uint8_t rndbuff[PLAT_RND_VECTOR_SZ],
			       uint8_t *scratch, uint32_t scratch_sz)
{
	uint32_t ret;
	paddr_t func_addr = romapi_getrndvector[get_api_table_index()];
	paddr_t rndbuff_pa = va2pa(rndbuff);
	paddr_t scratch_pa = va2pa(scratch);

	assert(scratch_sz >= 4096);
	assert(rndbuff_pa % 8 == 0);
	assert(scratch_pa % 8 == 0);

	cache_op_inner(DCACHE_AREA_CLEAN, rndbuff, PLAT_RND_VECTOR_SZ);
	cache_op_inner(DCACHE_AREA_CLEAN, scratch, scratch_sz);

	ret = plat_call_romapi(func_addr, rndbuff_pa, scratch_pa, scratch_sz);

	cache_op_inner(DCACHE_AREA_INVALIDATE, rndbuff, PLAT_RND_VECTOR_SZ);

	return ret;
}
