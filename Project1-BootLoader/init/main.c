#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define INT_IN_BYTES (4)
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define BOOT_LOADER_ENTRIES 0x50200000
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define APP_NUMBER_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define APP_INFO_SECTOR (BOOT_LOADER_SIG_OFFSET - 6)
#define APP_IN_MEMORY 0x54000000

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    // 1. get number from  0x50200000
    // #define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
    // #define APP_NUMBER_LOC (BOOT_LOADER_SIG_OFFSET - 4)
    // #define APP_INFO_SECTOR (BOOT_LOADER_SIG_OFFSET - 6)
    short appnum = *(short*)(BOOT_LOADER_ENTRIES + APP_NUMBER_LOC);
    bios_putstr("\napp_num: ");
    int appnum_size_first_half = (appnum / EI_NIDENT) >= 10?  appnum / EI_NIDENT - 10  + 'a': appnum / EI_NIDENT  + '0';
    int appnum_size_second_half = (appnum % EI_NIDENT) >= 10?  appnum % EI_NIDENT - 10  + 'a': appnum % EI_NIDENT  + '0';
    bios_putchar(appnum_size_first_half);
    bios_putchar(appnum_size_second_half);


    short os_size_count = *(short*)(BOOT_LOADER_ENTRIES + OS_SIZE_LOC);
    bios_putstr("\nos_size_count: ");
    int os_size_first_half = (os_size_count / EI_NIDENT) >= 10?  os_size_count / EI_NIDENT - 10  + 'a': os_size_count / EI_NIDENT  + '0';
    int os_size_second_half = (os_size_count % EI_NIDENT) >= 10?  os_size_count % EI_NIDENT - 10  + 'a': os_size_count % EI_NIDENT  + '0';
    bios_putchar(os_size_first_half);
    bios_putchar(os_size_second_half);

    
    short app_info_sector = *(short*)(BOOT_LOADER_ENTRIES + APP_INFO_SECTOR);
    bios_putstr("\napp_info_sector: ");
    int app_info_first_half = (app_info_sector / EI_NIDENT) >= 10?  app_info_sector / EI_NIDENT - 10  + 'a': app_info_sector / EI_NIDENT  + '0';
    int app_info_second_half = (app_info_sector % EI_NIDENT) >= 10?  app_info_sector % EI_NIDENT - 10  + 'a': app_info_sector % EI_NIDENT  + '0';
    bios_putchar(app_info_first_half);
    bios_putchar(app_info_second_half);
    bios_putstr(" \n");
    // with the growth of our main-code, the section in which app_info at, and the os_size is growthing

    // 2. sd_read of app-info sector, and relocate it to ...[assume we place it in 0x54000000]
    bios_sd_read(APP_IN_MEMORY, 1, app_info_sector);

    // 3. initialize tasks, and print the initializing result
    long address_location = APP_IN_MEMORY;
    for(int i = 0; i < (int)appnum + 1; i++){
        char temp_taskname[EI_NIDENT];
        for(int j = 0; j < EI_NIDENT; j++){
            temp_taskname[j] = *(char*)(address_location);
            address_location++;
        }
        strncmp(tasks[i].taskname, temp_taskname, strlen(temp_taskname));
        bios_putstr(temp_taskname);
        bios_putchar('\n');
        int temp_start_id = *(int*)(address_location);
        address_location += 4;
        tasks[i].start_block_id = temp_start_id;

        int temp_block_num = *(int *)(address_location);
        address_location += 4;
        tasks[i].total_block_num = temp_block_num;
    }
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // p1-task2
    bios_putstr("Input character, enter 0 to finish\n");
    while(1){
        int result = bios_getchar();
        if(result != -1){
            bios_putchar(result);
        }
        if(result == (int)'0'){
            break;
        }
    }
    bios_putstr("\nInput and output test finished!\n");
    bios_putstr("\nInput taskid\n");
    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    // while(1){
    //     int input_task_id = bios_getchar();
    //     if(input_task_id != -1){
    //         bios_putstr("The input task id is: ");
    //         bios_putchar(input_task_id);
    //         input_task_id = input_task_id - '0';
    //         long task_enterance_address = load_task_img(input_task_id);
    //         // This line is useful, but we find in a0, it stores the enterance address
    //         asm volatile("mv a7, %0\n"
    //         : :"r"(input_task_id));
    //         ( *(void(*)(void))task_enterance_address)();
    //     }
    // }
    short total_task_num = *(short*)(BOOT_LOADER_ENTRIES + APP_NUMBER_LOC);
    while(1){
        bios_putstr("\nInput taskname: limit in 16 bytes, if press * means end\n");
        char buf_taskname[EI_NIDENT] = {0};
        int count_taskname = 0;
        while(count_taskname < EI_NIDENT){
            int taskname_result = bios_getchar();
            if(taskname_result != -1 && taskname_result != '*'){
                bios_putchar(taskname_result);
                buf_taskname[count_taskname] = taskname_result;
                count_taskname++;
            }
            else if(taskname_result == '*'){
                break;
            }
        }
        long task_enterance_address = load_task_img_by_name((int)total_task_num, buf_taskname);
        ( *(void(*)(void))task_enterance_address)();
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
