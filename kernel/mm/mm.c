#include <os/mm.h>
#include <os/task.h>   // newly added!
#include <os/string.h> // newly added!
#include <os/kernel.h> // newly added!

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
    global_free_sentienl = (struct SentienlNode *)AVAILABLE_KERNEL;
    malloc_free_sentienl = (struct SentienlNode *)(AVAILABLE_KERNEL + (PAGE_SIZE / 2));
    
    struct ListNode *prev_node = (struct ListNode *)(global_free_sentienl -> head);

    for(uint64_t index = AVAILABLE_KERNEL + PAGE_SIZE; index < FREEMEM_KERNEL_END; index += PAGE_SIZE){
        uint64_t listnode_address = index;
        struct ListNode *current_node = (struct ListNode *)listnode_address;
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
    current_node -> next = global_free_sentienl -> head;
    global_free_sentienl -> head = current_node;  // insert from head
}

uintptr_t kmalloc() // remember pass number * PAGE_SIZE
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    static bool init_or_not = false;
    if(init_or_not == false){
        kinit();
        init_or_not = true;
    }
    // Step1:cacluate num of page
    int page_num = 1;

    // Step2: allocate new SentienlNode
    // place it at half of the page
    malloc_free_sentienl -> head = NULL;

    // Step3: traverse
    struct ListNode *global_temp = global_free_sentienl -> head;
    struct ListNode *prev_node = NULL;
    while(global_temp != NULL && page_num > 0){
        if(malloc_free_sentienl -> head == NULL){
            malloc_free_sentienl -> head = global_temp;
        }
        prev_node = global_temp;                // find the last prev to cut off 
        global_temp = global_temp -> next;      // global_free_sentienl ->[] -> [] -> [] -> [] -> [] | -> [] -> [] -> []
        page_num--;
    }
    prev_node -> next = NULL;
    global_free_sentienl -> head = global_temp;
    return malloc_free_sentienl -> head;
}

void print_page_alloc_info(struct SentienlNode *sentienl_head){
    struct ListNode *temp = sentienl_head -> head;
    while(temp != NULL){
        printl("0x%x -> ", (uintptr_t)temp);
        temp = temp -> next;
    }
    printl("NULL\n");
}

//**************************clean temporary mapping************************************************//
void init_clean_boot_address_map(){
    PTE *early_pgdir = (PTE *)pa2kva(PGDIR_PA);
    uint64_t va = 0x50000000lu;
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS));

    uint64_t physical_address = get_pa(early_pgdir[vpn2]);
    clear_pgdir(pa2kva(physical_address)); // clean 2th page_table

    early_pgdir[vpn2] = 0;
}

/**
this function is to map three level user_page
(1) level_one_pgdir, has been allocated
(2) level_two_pgdir, need to call kmalloc(1 * PAGE_SIZE)
(3) level_three_pgdir, need to call kmalloc(2 * PAGE_SIZE)

debug error: 
*********** first record *****************************************************
because we assign every page as struct ListNode *, 
so level_one_pgdir 0xffffffc052003000 looks like: 0x52003000 00000000 00000000 00000000
it did not pass the test that level_one_pgdir[vpn2] == 0

// usage: map_single_user_page(0x10000, 0x54000000, (PTE *)(pcb[0].user_pgdir_kva));
*/
void map_single_user_page(uint64_t va, uint64_t pa, PTE *level_one_pgdir){
    printl("\n\nmapping 0x%x to 0x%x\n", va, pa);
    printl("[map_single_user_page]: level_one_pgdir 0x%x\n", (uintptr_t)level_one_pgdir); // debug

    va &= VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = (va >> NORMAL_PAGE_SHIFT) & VPN0_MASK;

    if(level_one_pgdir[vpn2] == 0){ // have not allocated level_two_pgdir
        uint64_t return_level_two_address = kmalloc();
        printl("[map_single_user_page]: level_two_pgdir 0x%x\n", kva2pa(return_level_two_address)); // debug
        set_pfn(&level_one_pgdir[vpn2], kva2pa(return_level_two_address) >> NORMAL_PAGE_SHIFT);
        set_attribute(&level_one_pgdir[vpn2], _PAGE_PRESENT| _PAGE_USER);
        clear_pgdir(pa2kva(get_pa(level_one_pgdir[vpn2])));     // clean level_two_pgdir, for furture fullfill
    }
    PTE *level_two_pgdir = (PTE *)pa2kva(get_pa(level_one_pgdir[vpn2]));
    if(level_two_pgdir[vpn1] == 0){  // have not allocated level_three_pgdir
        uint64_t return_level_three_address = kmalloc();
        printl("[map_single_user_page]: level_three_pgdir 0x%x\n", kva2pa(return_level_three_address)); // debug
        set_pfn(&level_two_pgdir[vpn1], kva2pa(return_level_three_address) >> NORMAL_PAGE_SHIFT);
        set_attribute(&level_two_pgdir[vpn1], _PAGE_PRESENT | _PAGE_USER);
        clear_pgdir(pa2kva(get_pa(level_two_pgdir[vpn1])));     // clean level_three_pgdir, for furture fullfill
    }
    PTE *level_three_pgdir = (PTE *)pa2kva(get_pa(level_two_pgdir[vpn1]));
    set_pfn(&level_three_pgdir[vpn0], pa >> NORMAL_PAGE_SHIFT);
    set_attribute(
        &level_three_pgdir[vpn0], _PAGE_PRESENT | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC | _PAGE_USER);

    printl("[Verify]level_one_pgdir: 0x%x\n", kva2pa(level_one_pgdir));
    printl("[Verify]level_two_pgdir: 0x%x,\tfunction[vpn2:%d] result = 0x%x\n", kva2pa(level_two_pgdir), vpn2, get_pa(level_one_pgdir[vpn2]));
    printl("[Verify]level_three_pgdir: 0x%x,\tfunction[vpn1:%d] result = 0x%x\n", kva2pa(level_three_pgdir), vpn1, get_pa(level_two_pgdir[vpn1]));
    printl("[Verify]physical_frame: 0x%x,\tfunction[vpn0:%d] result = 0x%x\n\n\n", pa, vpn0, get_pa(level_three_pgdir[vpn0]));
}

/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(uintptr_t dest_pgdir, uintptr_t src_pgdir)
{
    // TODO [P4-task1] share_pgtable:
    memcpy(dest_pgdir, src_pgdir, PAGE_SIZE);
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

