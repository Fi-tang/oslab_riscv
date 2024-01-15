#include <os/mm.h>

// NOTE: A/C-core
static ptr_t kernMemCurr = FREEMEM_KERNEL;

ptr_t allocPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int numPage)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + numPage * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void kinit(){   // allocate from 0xffffffc052001000 to 0xfffffc060000000
    uint64_t sentienl_address = FREEMEM_KERNEL;
    global_free_sentienl = (struct SentienlNode *)FREEMEM_KERNEL;
    struct ListNode *prev_node = (struct ListNode *)(global_free_sentienl -> head);

    for(uint64_t index = FREEMEM_KERNEL + PAGE_SIZE; index < FREEMEM_KERNEL_END; index += PAGE_SIZE){
        uint64_t listnode_address = index;
        struct ListNode *current_node = (struct ListNode *)listnode_address;
        current_node -> virtual_address = index;
        current_node -> next = NULL;
        if(index == FREEMEM_KERNEL + PAGE_SIZE){
            global_free_sentienl -> head = current_node;
            prev_node = global_free_sentienl -> head;
        }
        else{
            prev_node -> next = current_node;
            prev_node = current_node;
        }
    }
}


void freePage(ptr_t baseAddr)   // assume 4 KB
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    ptr_t round_down_baseAddr = ROUNDDOWN(baseAddr, PAGE_SIZE);

    struct ListNode *current_node = (struct ListNode *)round_down_baseAddr;
    current_node -> virtual_address = ROUNDDOWN(baseAddr, PAGE_SIZE);
    current_node -> next = global_free_sentienl -> head;
    global_free_sentienl -> head = current_node;  // insert from head
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    static bool init_or_not = false;
    if(init_or_not == false){
        kinit();
        init_or_not = true;
    }

    uintptr_t actual_size = ROUND(size, PAGE_SIZE);
    int page_num = actual_size / PAGE_SIZE;

    // 1. get the traverse node from 0 to page_num
    struct SentienlNode *malloc_Node = global_free_sentienl;
    struct ListNode *traverse_node = global_free_sentienl -> head;
    for(int i = 0; i < page_num; i++){
        traverse_node = traverse_node -> next;
    }
    // 2. Assign NULL
    struct ListNode *new_head = traverse_node -> next;
    traverse_node -> next = NULL;
    global_free_sentienl -> head = new_head;
    // 3. return former head
    return malloc_Node;
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
}

/* allocate physical page for `va`, mapping it into `pgdir`,
   return the kernel virtual address for the page
   */
uintptr_t alloc_page_helper(uintptr_t va, uintptr_t pgdir)
{
    // TODO [P4-task1] alloc_page_helper:
}

uintptr_t shm_page_get(int key)
{
    // TODO [P4-task4] shm_page_get:
}

void shm_page_dt(uintptr_t addr)
{
    // TODO [P4-task4] shm_page_dt:
}