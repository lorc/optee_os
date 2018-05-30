// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2018, EPAM Systems. All rights reserved. */

#ifndef KERNEL_VIRTUALIZATION_H
#define KERNEL_VIRTUALIZATION_H

#include <stdbool.h>
#include <stdint.h>
#include <mm/core_mmu.h>

#define HYP_CLNT_ID 0

uint32_t virt_guest_created(uint16_t guest_id);
uint32_t virt_guest_destroyed(uint16_t guest_id);
bool virt_set_guest(uint16_t guest_id);
void virt_unset_guest(void);
void virt_on_stdcall(void);

void virt_init_memory(struct tee_mmap_region *memory_map);

struct tee_mmap_region *virt_get_memory_map(void);
void virt_get_ta_ram(vaddr_t *start, vaddr_t *end);

#endif	/* KERNEL_VIRTUALIZATION_H */
