/*
 * Copyright (c) 2016, GlobalLogic
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

#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

/* Make stacks aligned to data cache line length */
#define STACK_ALIGNMENT		64

#define GIC_BASE		0xF1000000
#define GICC_BASE		0xF1020000
#define GICD_BASE		0xF1010000

#define CONSOLE_UART_BASE	0xE6E88000

#ifdef CFG_WITH_PAGER

/* Emulated SRAM */
#define TZSRAM_BASE	        0x44100000
#define TZSRAM_SIZE		(768*1024)

#define TZDRAM_BASE		(TZSRAM_BASE + TZSRAM_SIZE)
#define TZDRAM_SIZE		(0x03D00000 - TZSRAM_SIZE)

#else /*CFG_WITH_PAGER*/

/* Location of trusted dram */
#define TZDRAM_BASE		0x44000000
#define TZDRAM_SIZE		0x03E00000

#endif	/* CFG_WITH_PAGER */

#if defined(PLATFORM_FLAVOR_salvator_h3)
#define CFG_TEE_CORE_NB_CORE	8
#define NSEC_DDR_0_BASE		0x47E00000
#define NSEC_DDR_0_SIZE		0x38200000
#define NSEC_DDR_1_BASE		0x500000000U
#define NSEC_DDR_1_SIZE		0x40000000
#define NSEC_DDR_2_BASE		0x600000000U
#define NSEC_DDR_2_SIZE		0x40000000
#define NSEC_DDR_3_BASE		0x700000000U
#define NSEC_DDR_3_SIZE		0x40000000

#elif defined(PLATFORM_FLAVOR_salvator_m3)
#define CFG_TEE_CORE_NB_CORE	4
#define NSEC_DDR_0_BASE		0x47E00000
#define NSEC_DDR_0_SIZE		0x78200000
#define NSEC_DDR_1_BASE		0x600000000U
#define NSEC_DDR_1_SIZE		0x80000000

#endif

/* Full GlobalPlatform test suite requires CFG_SHMEM_SIZE to be at least 2MB */
#define CFG_SHMEM_START		(TZDRAM_BASE + TZDRAM_SIZE)
#define CFG_SHMEM_SIZE		0x100000

#define CFG_TEE_RAM_VA_SIZE	(1536 * 1024)

#ifndef CFG_TEE_LOAD_ADDR
#define CFG_TEE_LOAD_ADDR	CFG_TEE_RAM_START
#endif

/*
 * Everything is in TZDRAM.
 * +------------------+
 * |        | TEE_RAM |
 * + TZDRAM +---------+
 * |        | TA_RAM  |
 * +--------+---------+
 */
#ifdef CFG_WITH_PAGER
#define CFG_TEE_RAM_PH_SIZE	(TZSRAM_SIZE)
#define CFG_TEE_RAM_START	(TZSRAM_BASE)
#define MAX_XLAT_TABLES		10
#else
#define CFG_TEE_RAM_PH_SIZE	CFG_TEE_RAM_VA_SIZE
#define CFG_TEE_RAM_START	(TZDRAM_BASE + 0x00100000)
#endif
#define CFG_TA_RAM_START	ROUNDUP((CFG_TEE_RAM_START +  \
					CFG_TEE_RAM_VA_SIZE), \
					CORE_MMU_DEVICE_SIZE)

#define CFG_WHOLE_TA_RAM_SIZE	ROUNDDOWN((TZDRAM_SIZE - CFG_TEE_RAM_VA_SIZE), \
					  CORE_MMU_DEVICE_SIZE)
#ifdef CFG_VIRTUALIZATION
#define VIRT_MAX_GUESTS		4
#define CFG_TA_RAM_SIZE		ROUNDDOWN(CFG_WHOLE_TA_RAM_SIZE / VIRT_MAX_GUESTS, \
					  CORE_MMU_DEVICE_SIZE)
#else
#define CFG_TA_RAM_SIZE		CFG_TA_RAM_SIZE
#endif
#endif /*PLATFORM_CONFIG_H*/
