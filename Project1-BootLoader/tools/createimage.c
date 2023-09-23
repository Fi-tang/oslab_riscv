#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EI_NIDENT (16)
#define INT_IN_BYTES (4)
#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define APP_NUMBER_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define APP_INFO_SECTOR (BOOT_LOADER_SIG_OFFSET - 6)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char taskname[EI_NIDENT];
    int start_block_id;
    int total_block_num;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 2;
    int nbytes_kernel = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w+");
    assert(img != NULL);

    /* for each input file */
    // long new_phyaddr_base = SECTOR_SIZE;
    int section_counter = 0;
    int task_id = 0;
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 2;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {
            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);

            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            write_segment(phdr, fp, img, &phyaddr);
         
            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }

            // here, we padding but just place every application at the beginning
            if(strcmp(*files, "bootblock") == 0){
                section_counter = 1;
                write_padding(img, &phyaddr, SECTOR_SIZE);
            }
            else{
                // Adding sectors count
                if(strcmp(*files, "main") == 0){
                    section_counter += 1;
                
                }
                printf("***current_filesz: %d and that accounts for %d sectors\n", NBYTES2SEC(get_filesz(phdr)), get_filesz(phdr));

                section_counter += NBYTES2SEC(get_filesz(phdr));
                // real section_id is section_counter - 1
                // [p1-task4]: padding to new section
                printf("***The section_counter = %d\n", section_counter);
                int new_paddingRange = SECTOR_SIZE * section_counter;
                write_padding(img, &phyaddr, new_paddingRange);
            }
            
            // init task_info_t, task_id[0] = main
            if(strcmp(*files, "bootblock") != 0){
                strncpy(taskinfo[task_id].taskname, *files, strlen(*files));
                taskinfo[task_id].total_block_num = NBYTES2SEC(get_filesz(phdr));
                if(strcmp(*files, "main") == 0){
                    taskinfo[task_id].start_block_id = 1;
                }
                else{
                    taskinfo[task_id].start_block_id = section_counter - NBYTES2SEC(get_filesz(phdr));
                }
                
                task_id++;
            }
        }
        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */
        // if (strcmp(*files, "bootblock") == 0) {
        //     write_padding(img, &phyaddr, SECTOR_SIZE);
        // }
        //else{
            // p1-task3, every application(including main), occupies 15 sectors
            // find error, the write_padding's new_phyaddr should be seriously calculated!
            // [p1-task3]: need addition for p1-task4
            // new_phyaddr_base += 15 * SECTOR_SIZE;
            // printf("  !!before padding :  This time's new_phyaddr_base = 0x%lx\n", new_phyaddr_base);
            // write_padding(img, &phyaddr, new_phyaddr_base);
        //}
        fclose(fp);
        files++;
    }
    printf("\n\n\n***debug... Entering write_img_info\n");
    write_img_info(nbytes_kernel, taskinfo, tasknum, img);

    fclose(img);

    // debug:
    printf("\n***Printing taskinfo\n");
    for(int i = 0; i < task_id; i++){
        printf("**taskinfo[%d].taskname = %s\ttaskinfo[%d].start_block_id=%d\ttaskinfo[%d].total_block_num=%d\n",
        i, taskinfo[i].taskname,  i,taskinfo[i].start_block_id,  i,taskinfo[i].total_block_num);
    }
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE * img)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    // image is not an elf file
    int ret_app_number, ret_os_size, ret_app_sector;
    fseek(img, APP_NUMBER_LOC, SEEK_SET);
    ret_app_number = fwrite(&tasknum, 2, 1, img);
    assert(ret_app_number == 1);

    rewind(img);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    int kernel_sectors = NBYTES2SEC(nbytes_kernel);
    ret_os_size = fwrite(&kernel_sectors, 2, 1, img);
    assert(ret_os_size == 1);

    int app_info_sector = 1 + (NBYTES2SEC(nbytes_kernel));
    fseek(img, APP_INFO_SECTOR, SEEK_SET);
    ret_app_sector = fwrite(&app_info_sector, 2, 1, img);
    assert(ret_app_sector == 1);
    //                                              a b                   c d                 e f
    //  0x1f0: 0000  0000  0000  0000  0000         0000                  0000                0000 
    //                                        [APP_INFO_SECTOR]      [APP_NUMBER_LOC]       [OS_SIZE_LOC]        
    // [p1-task4], writing task_info_related message in the sector after main
    long APP_INFO_ADDRESS = (NBYTES2SEC(nbytes_kernel) + 1) * SECTOR_SIZE;
    rewind(img);
    fseek(img, APP_INFO_ADDRESS, SEEK_SET);
    for(int i = 0; i < tasknum + 1; i++){
        int app_info_result;
        app_info_result = fwrite(&taskinfo[i], sizeof(task_info_t), 1, img);
        assert(app_info_result == 1);
    }

    // debug part A little revision, changing fopen(IMAGE_FILE, "w") to fopen(IMAGE_FILE, "w+")
    // the following can be negelected
    rewind(img);
    fseek(img, APP_NUMBER_LOC, SEEK_SET);
    printf("debug...:App_number = %d%d\n", fgetc(img), fgetc(img));

    rewind(img);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    printf("debug...:os_total_size = %d%d\n", fgetc(img), fgetc(img)); 

    rewind(img);
    fseek(img, APP_INFO_SECTOR, SEEK_SET);
    printf("debug...:App_info_sector = %d%d\n", fgetc(img), fgetc(img)); 

    rewind(img);
    fseek(img, APP_INFO_ADDRESS, SEEK_SET);
    // in memory:
    // char tasknum occupies 16 byte, start_id occupies 4 byte, and num_of_blocks occupies 4 byte
    for(int i = 0; i < tasknum + 1; i++){
        task_info_t *temp = (task_info_t *)malloc(sizeof(task_info_t));
        for(int j = 0; j < EI_NIDENT; j++){
            temp -> taskname[j] = fgetc(img);
        }
        // the next number should be getting 4 fgetc()
        int  real_start_block = 0;
        int  start_block_count = 1;
        for(int j = 0; j < INT_IN_BYTES; j++){
            int base_number = (int)fgetc(img);
            real_start_block = real_start_block + base_number * start_block_count;
            start_block_count = start_block_count << INT_IN_BYTES;
        }

        int real_num_block = 0;
        int num_block_count = 1;
        for(int j = 0; j < INT_IN_BYTES; j++){
            int base_number = (int)fgetc(img);
            real_num_block = real_num_block + base_number * num_block_count;
            num_block_count = num_block_count << INT_IN_BYTES;
        }

        printf("taskinfo[%d].taskname=%s\ttaskinfo[%d].start_block_id=%d\ttaskinfo[%d].num_block=%d\n",
        i, temp -> taskname, i, real_start_block, i, real_num_block);
    }
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}