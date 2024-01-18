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

void kinit(){   // allocate from 0xffffffc052002000 to 0xfffffc060000000
    uint64_t sentienl_address = AVAILABLE_KERNEL;
    global_free_sentienl = (struct SentienlNode *)AVAILABLE_KERNEL;
    struct ListNode *prev_node = (struct ListNode *)(global_free_sentienl -> head);

    for(uint64_t index = AVAILABLE_KERNEL + PAGE_SIZE; index < FREEMEM_KERNEL_END; index += PAGE_SIZE){
        uint64_t listnode_address = index;
        struct ListNode *current_node = (struct ListNode *)listnode_address;
        current_node -> page_address = index;
        current_node -> next = NULL;
        if(index == AVAILABLE_KERNEL + PAGE_SIZE){
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
    current_node -> page_address = ROUNDDOWN(baseAddr, PAGE_SIZE);
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

    if(size == 0) return NULL;
    // Step1:cacluate num of page
    uint64_t round_up_alloc = ROUND(size, PAGE_SIZE);
    int page_num = round_up_alloc / PAGE_SIZE;

    // Step2: allocate new SentienlNode
    // place it at half of the page
    uint64_t malloc_addr = AVAILABLE_KERNEL + 8;
    struct SentienlNode *malloc_free = (struct SentienlNode *)malloc_addr;
    malloc_free -> head = NULL;

    // Step3: traverse
    struct ListNode *global_temp = global_free_sentienl -> head;
    struct ListNode *prev_node = NULL;
    while(global_temp != NULL && page_num > 0){
        if(malloc_free -> head == NULL){
            malloc_free -> head = global_temp;
        }
        prev_node = global_temp;                // find the last prev to cut off 
        global_temp = global_temp -> next;      // global_free_sentienl ->[] -> [] -> [] -> [] -> [] | -> [] -> [] -> []
        page_num--;
    }
    prev_node -> next = NULL;
    global_free_sentienl -> head = global_temp;
    return malloc_free;
}

void print_page_alloc_info(struct SentienlNode *sentienl_head){
    struct ListNode *temp = sentienl_head -> head;
    while(temp != NULL){
        printl("0x%x -> ", temp -> page_address);
        temp = temp -> next;
    }
    printl("NULL\n");
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