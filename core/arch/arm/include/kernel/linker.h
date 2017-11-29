/*
 * Copyright (c) 2017, Linaro Limited
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
#ifndef __KERNEL_LINKER_H
#define __KERNEL_LINKER_H

#include <initcall.h>
#include <kernel/dt.h>
#include <kernel/early_ta.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_mmu.h>
#include <types_ext.h>

/*
 * Symbols exported by the link script.
 */


/*
 * These addresses will be the start or end of the exception binary search
 * index table (.ARM.exidx section)
 */
extern const uint8_t __exidx_start[];
extern const uint8_t __exidx_end[];
extern const uint8_t __extab_start[];
extern const uint8_t __extab_end[];

extern const struct pseudo_ta_head __start_ta_head_section;
extern const struct pseudo_ta_head __stop_ta_head_section;

extern const struct core_mmu_phys_mem __start_phys_sdp_mem_section;
extern const struct core_mmu_phys_mem __end_phys_sdp_mem_section;
extern const struct core_mmu_phys_mem __start_phys_mem_map_section;
extern const struct core_mmu_phys_mem __end_phys_mem_map_section;
extern const struct core_mmu_phys_mem __start_phys_nsec_ddr_section;
extern const struct core_mmu_phys_mem __end_phys_nsec_ddr_section;

#define VCORE_UNPG_RX_PA	((unsigned long)__vcore_unpg_rx_start)
#define VCORE_UNPG_RX_SZ	((size_t)__vcore_unpg_rx_size)
#define VCORE_UNPG_RO_PA	((unsigned long)__vcore_unpg_ro_start)
#define VCORE_UNPG_RO_SZ	((size_t)__vcore_unpg_ro_size)
#define VCORE_UNPG_RW_PA	((unsigned long)__vcore_unpg_rw_start)
#define VCORE_UNPG_RW_SZ	((size_t)__vcore_unpg_rw_size)
#define VCORE_UNPG_KERN_RW_PA	((unsigned long)__vcore_unpg_kern_rw_start)
#define VCORE_UNPG_KERN_RW_SZ	((size_t)__vcore_unpg_kern_rw_size)
#define VCORE_INIT_RX_PA	((unsigned long)__vcore_init_rx_start)
#define VCORE_INIT_RX_SZ	((size_t)__vcore_init_rx_size)
#define VCORE_INIT_RO_PA	((unsigned long)__vcore_init_ro_start)
#define VCORE_INIT_RO_SZ	((size_t)__vcore_init_ro_size)
extern const uint8_t __vcore_unpg_rx_start[];
extern const uint8_t __vcore_unpg_rx_size[];
extern const uint8_t __vcore_unpg_ro_start[];
extern const uint8_t __vcore_unpg_ro_size[];
extern const uint8_t __vcore_unpg_rw_start[];
extern const uint8_t __vcore_unpg_rw_size[];
extern const uint8_t __vcore_unpg_kern_rw_start[];
extern const uint8_t __vcore_unpg_kern_rw_size[];
extern const uint8_t __vcore_init_rx_start[];
extern const uint8_t __vcore_init_rx_size[];
extern const uint8_t __vcore_init_ro_start[];
extern const uint8_t __vcore_init_ro_size[];

extern const uint8_t __text_start[];
extern const uint8_t __end[];

extern const initcall_t __initcall_start;
extern const initcall_t __initcall_end;

extern const uint8_t __data_start[];
extern const uint8_t __data_end[];
extern const uint8_t __rodata_start[];
extern const uint8_t __rodata_end[];
extern const uint8_t __bss_start[];
extern const uint8_t __bss_end[];
extern const uint8_t __nozi_start[];
extern const uint8_t __nozi_end[];
extern const uint8_t __nozi_stack_start[];
extern const uint8_t __nozi_stack_end[];
extern const uint8_t __init_start[];
extern const uint8_t __init_size[];
extern const uint8_t __tmp_hashes_start[];
extern const uint8_t __tmp_hashes_size[];

extern uint8_t __heap1_start[];
extern const uint8_t __heap1_end[];
extern uint8_t __heap2_start[];
extern const uint8_t __heap2_end[];

extern uint8_t __kheap_start[];
extern const uint8_t __kheap_end[];

extern const uint8_t __pageable_part_start[];
extern const uint8_t __pageable_part_end[];
extern const uint8_t __pageable_start[];
extern const uint8_t __pageable_end[];

#define ASAN_SHADOW_PA	((paddr_t)__asan_shadow_start)
#define ASAN_SHADOW_SZ	((size_t)__asan_shadow_size)
extern const uint8_t __asan_shadow_start[];
extern const uint8_t __asan_shadow_end[];
extern const uint8_t __asan_shadow_size[];

#define ASAN_MAP_PA	((paddr_t)__asan_map_start)
#define ASAN_MAP_SZ	((size_t)__asan_map_size)
extern const uint8_t __asan_map_start[];
extern const uint8_t __asan_map_end[];
extern const uint8_t __asan_map_size[];

extern const vaddr_t __ctor_list;
extern const vaddr_t __ctor_end;


extern const struct dt_driver __rodata_dtdrv_start;
extern const struct dt_driver __rodata_dtdrv_end;

extern const struct early_ta __rodata_early_ta_start;
extern const struct early_ta __rodata_early_ta_end;

/* Generated by core/arch/arm/kernel/link.mk */
extern const char core_v_str[];

#endif /*__KERNEL_LINKER_H*/

