#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>

#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9

#define SATP_ASID_SHIFT 44lu
#define SATP_MODE_SHIFT 60lu

#define NORMAL_PAGE_SHIFT 12lu
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT)  // 4KB
#define LARGE_PAGE_SHIFT 21lu
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT)   // 2MB

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT (1 << 0)
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access \
                                 */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT (1 << 8)     /* Reserved for software */

#define _PAGE_PFN_SHIFT 10lu

#define VA_MASK ((1lu << 39) - 1)

#define PPN_BITS 9lu
#define NUM_PTE_ENTRY (1 << PPN_BITS)

typedef uint64_t PTE;

/* Translation between physical addr and kernel virtual addr */
static inline uintptr_t kva2pa(uintptr_t kva)
{
    /* TODO: [P4-task1] */
    return kva - 0xffffffc000000000lu;
}

static inline uintptr_t pa2kva(uintptr_t pa)
{
    /* TODO: [P4-task1] */
    return pa + 0xffffffc000000000lu;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(PTE entry)
{
    /* TODO: [P4-task1] */
    uint64_t entry_pfn_releated = entry >> _PAGE_PFN_SHIFT;
    uint64_t extended_Mask = (1lu << 44) - 1;
    uint64_t physical_frame_number = entry_pfn_releated & extended_Mask;
    return physical_frame_number << NORMAL_PAGE_SHIFT;
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(PTE entry)
{
    /* TODO: [P4-task1] */
    // Step 1: shift 10 byte, to clean flag information: entry >> 10
    uint64_t entry_pfn_releated = entry >> _PAGE_PFN_SHIFT;
    // Step 2: clean the highest [53 44]Byte, extended_Mask = [53 (all 0) 44][43 (remain the same) 0]
    uint64_t extended_Mask = (1lu << 44) - 1;
    return (entry_pfn_releated & extended_Mask);
}

/**
Sv39's PTE:
[63 Reserved 54][53 PPN(2) 28][27 PPN(1) 19][18 PPN(0) 10][9  RSW  8][7 D][A][G][U][X][W][R][V 0]
*/
static inline void set_pfn(PTE *entry, uint64_t pfn)
{
    /* TODO: [P4-task1] */
    // Step1: clean entry's context, clear all but remain the lowest 10 bit's flag unchanged
    /**
    before:             [XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX XXXX]
    & (1 << 10) - 1 =   [0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0011 1111 1111]
    after:              [0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 00XX XXXX XXXX]
    */
    uint64_t clear_entry = (*entry) & ((1lu << _PAGE_PFN_SHIFT) - 1);
    // Step2: shift pfn << 10, fill physical frame number
    clear_entry |= (pfn << _PAGE_PFN_SHIFT);
    *entry = clear_entry;
}

/* Get/Set attribute(s) of the `entry` */
// [Question]: what does the mask actually mean?
/**
assume here that, mask means which position do we want to get?
mask means 0 to 9 
for example: mask[0]: V, mask[1]: R, mask[2]: W, mask[3]: X
             mask[4]: U, mask[5]: G, mask[6]: A, mask[7]: D
             mask[8]: Reserved mask[9]: Reserved
*/
static inline long get_attribute(PTE entry, uint64_t mask)
{
    /* TODO: [P4-task1] */
    uint64_t mask_shift = 1lu << mask;
    return (entry & mask_shift);
}
static inline void set_attribute(PTE *entry, uint64_t bits)
{
    /* TODO: [P4-task1] */
    // Step: only need or operation, |bits
    *entry = (*entry) | bits;
}

static inline void clear_pgdir(uintptr_t pgdir_addr) // virtual address
{
    /* TODO: [P4-task1] */
    // function: clean the whole page_directory's page
   char *clear_address = (char *)pgdir_addr;
   for(int i = 0; i < NORMAL_PAGE_SIZE; i++){
        *clear_address++ = 0;
   }
}
#endif  // PGTABLE_H