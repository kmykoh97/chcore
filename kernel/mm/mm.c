/*
 * Copyright (c) 2020 Institute of Parallel And Distributed Systems (IPADS), Shanghai Jiao Tong University (SJTU)
 * OS-Lab-2020 (i.e., ChCore) is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *   http://license.coscl.org.cn/MulanPSL
 *   THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 *   PURPOSE.
 *   See the Mulan PSL v1 for more details.
 */
#ifdef CHCORE
#include <common/util.h>
#include <common/kmalloc.h>
#endif

#include <common/mm.h>
#include <common/kprint.h>
#include <common/macro.h>

#include "buddy.h"
#include "slab.h"
#include "page_table.h"

extern unsigned long *img_end;
struct global_mem global_mem;


#define PHYSICAL_MEM_START (24*1024*1024) //24M

#define START_VADDR phys_to_virt(PHYSICAL_MEM_START) //24M
#define NPAGES (128*1000)

#define PHYSICAL_MEM_END (PHYSICAL_MEM_START+NPAGES*BUDDY_PAGE_SIZE)

extern void parse_mem_map(void *);
extern void arch_mm_init(void);

int physmem_map_num;
u64 physmem_map[8][2];

/*
 * Layout:
 *
 * | metadata (npages * sizeof(struct page)) | start_vaddr ... (npages * PAGE_SIZE) |
 *
 */

unsigned long get_ttbr1()
{
	unsigned long pgd;
	__asm__ ("mrs %0,ttbr1_el1" : "=r"(pgd));
	return pgd;
}

#define GET_PADDR_IN_PTE(entry) \
	(((u64)entry->table.next_table_addr) << PAGE_SHIFT)
#define GET_NEXT_PTP(entry) phys_to_virt(GET_PADDR_IN_PTE(entry))

#define NORMAL_PTP (0)
#define BLOCK_PTP  (1)

static int get_next_ptp(ptp_t *cur_ptp, u32 level, vaddr_t va,
			ptp_t **next_ptp, pte_t **pte, bool alloc)
{
	u32 index = 0;
	pte_t *entry;

	switch (level) {
	case 0:
		index = GET_L0_INDEX(va);
		break;
	case 1:
		index = GET_L1_INDEX(va);
		break;
	case 2:
		index = GET_L2_INDEX(va);
		break;
	case 3:
		index = GET_L3_INDEX(va);
		break;
	default:
		BUG_ON(1);
	}

	entry = &(cur_ptp->ent[index]);
	if (IS_PTE_INVALID(entry->pte)) {
		if (alloc == false) {
			// return -ENOMAPPING;
		}
		else {
			/* alloc a new page table page */
			ptp_t *new_ptp;
			paddr_t new_ptp_paddr;
			pte_t new_pte_val;

			/* alloc a single physical page as a new page table page */
			new_ptp = get_pages(0);
			BUG_ON(new_ptp == NULL);
			memset((void *)new_ptp, 0, PAGE_SIZE);
			new_ptp_paddr = virt_to_phys((vaddr_t)new_ptp);

			new_pte_val.pte = 0;
			new_pte_val.table.is_valid = 1;
			new_pte_val.table.is_table = 1;
			new_pte_val.table.next_table_addr
				= new_ptp_paddr >> PAGE_SHIFT;

			/* same effect as: cur_ptp->ent[index] = new_pte_val; */
			entry->pte = new_pte_val.pte;
		}
	}

	*next_ptp = (ptp_t *)GET_NEXT_PTP(entry);
	*pte = entry;
	if (IS_PTE_TABLE(entry->pte))
		return NORMAL_PTP;
	else
		return BLOCK_PTP;
}

/*
 * map_kernel_space: map the kernel virtual address 
 * [va:va+size] to physical addres [pa:pa+size].
 * 1. get the kernel pgd address
 * 2. fill the block entry with corresponding attribution bit
 * 
 */
void map_kernel_space(vaddr_t va, paddr_t pa, size_t len)
{
	//lab2
	ptp_t *l0_ptp, *l1_ptp, *l2_ptp, *l3_ptp;
	pte_t *pte;
	int ret;
	vaddr_t *pgtbl = (vaddr_t *)(get_ttbr1());

	size_t n = len/2097152;

	for (int i = 0; i < n; i++) {
		// L0 page table
		l0_ptp = (ptp_t *)pgtbl;
		ret = get_next_ptp(l0_ptp, 0, va, &l1_ptp, &pte, true);

		if (ret < 0) {
			// return ret;
		}

		// L1 page table
		ret = get_next_ptp(l1_ptp, 1, va, &l2_ptp, &pte, true);

		if (ret < 0) {
			// return ret;
		}

		// L2 page table
		ret = get_next_ptp(l2_ptp, 2, va, &l3_ptp, &pte, true);

		if (ret < 0) {
			// return ret;
		}

		// pte->pte = 0;
		// pte->l2_block.AP = ARM64_MMU_ATTR_PAGE_AP_HIGH_RW_EL0_RW;
		pte->l3_page.pfn = pa >> PAGE_SHIFT;
		pte->l2_block.UXN = 1;
		pte->l2_block.AF = 1;
		pte->l2_block.SH = 3;
		pte->l2_block.attr_index = 4;
		pte->l2_block.is_valid = 1;
		pte->l2_block.is_table = 0;
		va += 0x200000;
		pa += 0x200000;
	}
}


void kernel_space_check()
{
	unsigned long kernel_val;
	for(unsigned long  i = 64; i < 128; i++)
	{
		kernel_val = *(unsigned long *)(KBASE + (i << 21));
		kinfo("kernel_val: %lx\n", kernel_val);
	}
	kinfo("kernel space check pass\n");
}

void mm_init(void *info)
{
	vaddr_t free_mem_start = 0;
	vaddr_t free_mem_end = 0;
	struct page *page_meta_start = NULL;
	u64 npages = 0;
	u64 start_vaddr = 0;

	physmem_map_num = 0;

	// TODO: make parse_memory solid
	/* XXX: only use the last entry (biggest free chunk) */
	parse_mem_map(info);

	if (physmem_map_num == 1) {
		free_mem_start = phys_to_virt(physmem_map[0][0]);
		free_mem_end   = phys_to_virt(physmem_map[0][1]);

		npages = (free_mem_end - free_mem_start) /
			 (PAGE_SIZE + sizeof(struct page));
		start_vaddr = (free_mem_start + npages * sizeof(struct page));
		start_vaddr = ROUND_UP(start_vaddr, PAGE_SIZE);
		kdebug("[CHCORE] mm: free_mem_start is 0x%lx, free_mem_end is 0x%lx\n",
		       free_mem_start, free_mem_end);
	} else if (physmem_map_num == 0) {
		free_mem_start = phys_to_virt(ROUND_UP((vaddr_t)(&img_end), PAGE_SIZE));
		// free_mem_end = phys_to_virt(PHYSICAL_MEM_END);
		npages = NPAGES;
		start_vaddr = START_VADDR;
		kdebug("[CHCORE] mm: free_mem_start is 0x%lx, free_mem_end is 0x%lx\n",
			free_mem_start, phys_to_virt(PHYSICAL_MEM_END));
	} else {
		BUG("Unsupport physmem_map_num\n");
	}

	if ((free_mem_start + npages * sizeof(struct page)) > start_vaddr) {
		BUG("kernel panic: init_mm metadata is too large!\n");
	}

	page_meta_start = (struct page *)free_mem_start;
	kdebug("page_meta_start: 0x%lx, real_start_vadd: 0x%lx,"
	       "npages: 0x%lx, meta_page_size: 0x%lx\n",
	       page_meta_start, start_vaddr, npages, sizeof(struct page));

	/* buddy alloctor for managing physical memory */
	init_buddy(&global_mem, page_meta_start, start_vaddr, npages);

	/* slab alloctor for allocating small memory regions */
	init_slab();

	/* init PCID */
	arch_mm_init();

	map_kernel_space(KBASE + (64UL<< 21), 64UL<< 21, 64UL<<21);
	//check whether kernel space [KABSE + 128 : KBASE + 256] is mapped 
	// kernel_space_check();
}
