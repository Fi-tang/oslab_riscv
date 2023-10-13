#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50
#define BOOT_LOADER_ADDRESS 0x50200000
#define EI_NIDENT (16)
#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define APP_NUMBER_LOC (BOOT_LOADER_SIG_OFFSET - 4)

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
    bios_sd_read(&tasks, 2, 1);
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

    bios_putstr("\n\rInput test, press 0 to finish!\n");
    int putchar_test_result;
    while(1){
        putchar_test_result = bios_getchar();
        if(putchar_test_result != -1){
            bios_putchar(putchar_test_result);
        }
        if(putchar_test_result == (int)'0'){
            break;
        }
    }

    bios_putstr("\n***************Input testing passed! Task testing begin***********\n");
    short task_num = *(short *)(BOOT_LOADER_ADDRESS + APP_NUMBER_LOC);
    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    while(1){
        char input_task_name[EI_NIDENT];
        int task_name_count_index = 0;
        while(task_name_count_index < EI_NIDENT){
            int task_name_bios_getchar;
            task_name_bios_getchar = bios_getchar();
            if(task_name_bios_getchar != -1){
                if(task_name_bios_getchar != (int)'*'){
                    input_task_name[task_name_count_index] = task_name_bios_getchar;
                    bios_putchar(task_name_bios_getchar);
                    task_name_count_index++;
                }
                else{
                    input_task_name[task_name_count_index] = '\0';
                    break;
                }
            }
        } 
        bios_putchar('\n');
        long current_task_filesz = load_task_img_filesz(task_num, input_task_name);
        long current_task_memorysz = load_task_img_memorysz(task_num, input_task_name);
        long current_task_entry_address = load_task_img_by_name(task_num, input_task_name);
        long count_bss = 2 * (current_task_memorysz - current_task_filesz);
        unsigned char *clean_bss_ptr = NULL;
        clean_bss_ptr = (char*)(current_task_entry_address + current_task_filesz);
        while(count_bss--){
            *clean_bss_ptr = (unsigned char)0;
            clean_bss_ptr++;
        }	

        ( *(void(*)(void))current_task_entry_address)();
        
    }

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
