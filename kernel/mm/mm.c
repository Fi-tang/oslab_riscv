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

void kinit(){   // init from 0x52001000 to 0x60000000
    // Step 1: allocate global_available_header 's address
    uint64_t header_address = FREEMEM_KERNEL;
    global_available_header = (struct global_header *)header_address;
    // Step 2: link list
    struct available_node *prev = global_available_header -> available_list;
    for(uint64_t index = FREEMEM_KERNEL + PAGE_SIZE; index < FREEMEM_KERNEL_END; index += PAGE_SIZE){
        uint64_t virtual_address = index;
        struct available_node *current = (struct available_node *)virtual_address;
        current -> virtual_address = index;
        current -> next = NULL;
        prev -> next = current;
        prev = current;
    }
}


void freePage(ptr_t baseAddr)   // assume 4 KB
{
    // TODO [P4-task1] (design you 'freePage' here if you need):
    // Step1: change the baseAddr to round_down, for example [0 - 4096] --> freePage(4023) = freePage(0)
    ptr_t round_down_baseAddr = ROUNDDOWN(baseAddr, PAGE_SIZE);
    // change it to struct available_node's type
    struct available_node *free_node = (struct available_node *)round_down_baseAddr;
    free_node -> virtual_address = ROUNDDOWN(baseAddr, PAGE_SIZE);

    free_node -> next = global_available_header -> available_list;
    global_available_header -> available_list = freenode;
    // add it to the head of global_available
}

void *kmalloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    static bool init_or_not = false;
    if(init_or_not == false){ 
        kinit();
        init_or_not = true;
    }

    uintptr_t round_up_page = ROUND(size);      // need to allocate how many pages
    int page_num = round_up_page / PAGE_SIZE;   // need to get page_num's pages

    // need to allocate page_num's pages
    // [old_header ]-> o[] -> o[] -> o[] -> o[] -> o[] -> n[] -> n[] -> n[]
    // [new_header] ------------------------------------> n[] -> n[] -> n[]
    // [allocate_header] -> o[] -> o[] -> o[] -> o[] -> o[]-> NULL
    struct global_header *malloc_node = global_available_header;
    struct available_node *traverse_node = global_available_header -> available_list;
    for(int i = 0; i < page_num; i++){
        traverse_node = traverse_node -> next;
    }
    struct available_node *new_node = traverse_node -> next;
    traverse_node -> next = NULL;
    global_available_header -> available_list = new_node;
    return (void *)(malloc_node -> available_list);
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