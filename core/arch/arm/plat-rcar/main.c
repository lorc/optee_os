// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2016, GlobalLogic
 * Copyright (c) 2015-2018, Renesas Electronics Corporation
 * Copyright (c) 2021, EPAM Systems
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

#include <console.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <io.h>
#include <platform_config.h>
#include <stdint.h>
#include <drivers/scif.h>
#include <drivers/gic.h>

#include "rcar.h"

register_phys_mem_pgdir(MEM_AREA_IO_SEC, CONSOLE_UART_BASE, SCIF_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, GICD_BASE, GIC_DIST_REG_SIZE);
register_phys_mem_pgdir(MEM_AREA_IO_SEC, GICC_BASE, GIC_DIST_REG_SIZE);

/* Actually we need only register in both next regions */
register_phys_mem(MEM_AREA_IO_SEC, RCAR_PRODUCT_REGISTER & ~SMALL_PAGE_MASK,
		  SMALL_PAGE_SIZE);
register_phys_mem(MEM_AREA_IO_SEC, RCAR_FUSE_DUMMY5 & ~SMALL_PAGE_MASK,
		  SMALL_PAGE_SIZE);

register_dynamic_shm(NSEC_DDR_0_BASE, NSEC_DDR_0_SIZE);
register_dynamic_shm(NSEC_DDR_1_BASE, NSEC_DDR_1_SIZE);
#ifdef NSEC_DDR_2_BASE
register_dynamic_shm(NSEC_DDR_2_BASE, NSEC_DDR_2_SIZE);
#endif
#ifdef NSEC_DDR_3_BASE
register_dynamic_shm(NSEC_DDR_3_BASE, NSEC_DDR_3_SIZE);
#endif

static struct scif_uart_data console_data __nex_bss;

enum rcar_product_type rcar_get_product_type(void)
{
	static enum rcar_product_type rcar_product_type __nex_bss;
	uint32_t prr;
	vaddr_t prr_va;

	if (rcar_product_type != RCAR_SOC_UNKNOWN)
		return rcar_product_type;

	if (cpu_mmu_enabled())
		prr_va = (vaddr_t)phys_to_virt_io(RCAR_PRODUCT_REGISTER);
	else
		prr_va = RCAR_PRODUCT_REGISTER;

	prr = io_read32(prr_va);

	switch (prr & PRR_PRODUCT_MASK) {
	case PRR_PRODUCT_H3:
		switch (prr & PRR_CUT_MASK) {
		case PRR_CUT_10:
			rcar_product_type = RCAR_H3_ES1_0;
			break;
		case PRR_CUT_11:
			rcar_product_type = RCAR_H3_ES1_1;
			break;
		case PRR_CUT_20:
			rcar_product_type = RCAR_H3_ES2_0;
			break;
		case PRR_CUT_30:
			rcar_product_type = RCAR_H3_ES3_0;
			break;
		default:
			/* Will panic at end of the function */
			break;
		}
		break;
	case PRR_PRODUCT_M3:
		switch (prr & PRR_CUT_MASK) {
		case PRR_CUT_10:
		{
			uint32_t dummy5;
			vaddr_t d5_va;

			if (cpu_mmu_enabled())
				d5_va =
				   (vaddr_t)phys_to_virt_io(RCAR_FUSE_DUMMY5);
			else
				d5_va = RCAR_FUSE_DUMMY5;

			dummy5 = io_read32(d5_va);
			switch (dummy5) {
			case DUMMY5_M3_100:
				rcar_product_type = RCAR_M3_ES1_00;
				break;
			case DUMMY5_M3_105:
				rcar_product_type = RCAR_M3_ES1_05;
				break;
			case DUMMY5_M3_106:
				rcar_product_type = RCAR_M3_ES1_06;
				break;
			default:
				/* Will panic at end of the function */
				break;
			}
			break;
		}
		case PRR_CUT_11:
			rcar_product_type = RCAR_M3_ES1_1;
			break;
		default:
			/* Will panic at end of the function */
			break;
		}
		break;
	case PRR_PRODUCT_V3M:
		switch (prr & PRR_CUT_MASK) {
		case PRR_CUT_10:
			rcar_product_type = RCAR_V3M_ES_1_0;
			break;
		case PRR_CUT_20:
			rcar_product_type = RCAR_V3M_ES_2_0;
			break;
		default:
			/* Will panic at end of the function */
			break;
		}
		break;
	case PRR_PRODUCT_M3N:
		rcar_product_type = RCAR_M3N;
		break;
	case PRR_PRODUCT_V3H:
		rcar_product_type = RCAR_V3H;
		break;
	case PRR_PRODUCT_E3:
		rcar_product_type = RCAR_E3;
		break;
	case PRR_PRODUCT_D3:
		rcar_product_type = RCAR_D3;
		break;
	default:
		/* Will panic at end of the function */
		break;
	}

	if (rcar_product_type == RCAR_SOC_UNKNOWN)
		panic("Can't determine Rcar SoC type");

	DMSG("Running on Rcar %s", rcar_product_str(rcar_product_type));

	return rcar_product_type;
}

void console_init(void)
{
	scif_uart_init(&console_data, CONSOLE_UART_BASE);
	register_serial_console(&console_data.chip);
}
