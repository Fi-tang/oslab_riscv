#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define EI_NIDENT (16)
#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define APP_NUMBER_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char taskname[EI_NIDENT];
    int start_block_id;
    int total_block_num;
    long task_filesz;
    long task_memorysz;
    long task_blockstart_offset;
} task_info_t;

#define TASK_MAXNUM 48
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
    int taskidx = 0;
    int total_filesz = 0;
    int total_blocknumber = 0;
    for (int fidx = 0; fidx < nfiles; ++fidx) {
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
            
            if (strcmp(*files, "bootblock") == 0) {
                write_padding(img, &phyaddr, 3 * SECTOR_SIZE);
                total_filesz += 3 * SECTOR_SIZE;
                total_blocknumber = 3;
            }
            /* update nbytes_kernel */
            else{
                if(strcmp(*files, "main") == 0){
                    nbytes_kernel += get_filesz(phdr);
                }
                /**
                 * 1. judge start_block_id
                 * if total_filesz < total_blocknumber * SECTOR_SIZE --> total_blocknumber - 1
                 * else total_blocknumber
                */
                if(total_filesz < total_blocknumber * SECTOR_SIZE){
                    taskinfo[taskidx].start_block_id = total_blocknumber - 1;
                    taskinfo[taskidx].task_blockstart_offset =  total_filesz - (total_blocknumber - 1) * SECTOR_SIZE;
                    int gap = total_blocknumber * SECTOR_SIZE - total_filesz;
                    if(get_filesz(phdr) <= gap){
                        taskinfo[taskidx].total_block_num = 1;
                    }
                    else{
                        taskinfo[taskidx].total_block_num = 1 + NBYTES2SEC(get_filesz(phdr) - gap);
                    }
                }
                else{
                    taskinfo[taskidx].start_block_id = total_blocknumber;
                    taskinfo[taskidx].task_blockstart_offset = 0;
                    taskinfo[taskidx].total_block_num = NBYTES2SEC(get_filesz(phdr));
                }
                strcpy(taskinfo[taskidx].taskname, *files);
                taskinfo[taskidx].task_filesz = get_filesz(phdr);
                taskinfo[taskidx].task_memorysz = get_memsz(phdr);

                total_filesz += get_filesz(phdr);
                total_blocknumber = NBYTES2SEC(total_filesz);
                taskidx++;
            }
            printf("filename = %s total_filesz = %d total_blocknumber = %d\n", *files, total_filesz, total_blocknumber);
            
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */

        fclose(fp);
        files++;
    }
    write_img_info(nbytes_kernel, taskinfo, tasknum, img);

    fclose(img);
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
    printf("\n****debugging sector:[write_img_info]***\n");
    int os_size_in_sectors = NBYTES2SEC(nbytes_kernel);
    rewind(img);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    int write_os_size;
    write_os_size = fwrite(&os_size_in_sectors, 4, 1, img);
    assert(write_os_size == 1);

    rewind(img);
    int write_app_number;
    fseek(img, APP_NUMBER_LOC, SEEK_SET);
    write_app_number = fwrite(&tasknum, 2, 1, img);
    assert(write_app_number == 1);
    // here, os_size has 4 bytes, while app_number has 2 bytes
    // [][][][] [][][][] [][]   [0][4]               [0][6] low |   [0][0] high
    //                          [App-number]       [os_size_in_sectors]   

    rewind(img);
    int write_task_info;
    fseek(img, SECTOR_SIZE, SEEK_SET);
    for(int i = 0; i < (tasknum + 1); i++){
        write_task_info = fwrite(&taskinfo[i], sizeof(task_info_t), 1, img);
        assert(write_task_info == 1);
    }

    //*********************************debugging
    rewind(img);
    int img_read_os_size;
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    fread(&img_read_os_size, sizeof(int), 1, img);
    printf("img_read_os_size = %d\n", img_read_os_size);

    rewind(img);
    short img_read_app_number;
    fseek(img, APP_NUMBER_LOC, SEEK_SET);
    fread(&img_read_app_number, sizeof(short), 1, img);
    printf("img_read_app_number = %d\n", img_read_app_number);

    rewind(img);
    fseek(img, SECTOR_SIZE, SEEK_SET);
    for(int i = 0; i < tasknum + 1; i++){
        char read_taskname[EI_NIDENT];
        for(int j = 0; j < EI_NIDENT; j++){
            read_taskname[j] = fgetc(img);
        }
        int read_start_block_id;
        fread(&read_start_block_id, sizeof(int), 1, img);
        int read_total_block_num;
        fread(&read_total_block_num, sizeof(int), 1, img);

        long read_task_filesz;
        fread(&read_task_filesz, sizeof(long), 1, img);
        long read_memory_filesz;
        fread(&read_memory_filesz, sizeof(long), 1, img);
        long read_task_blockstart_offset;
        fread(&read_task_blockstart_offset, sizeof(long), 1, img);
        printf("tasks[%d].taskname=%s\ttasks[%d].start_block_id=%d\n",
        i, read_taskname, i, read_start_block_id);
        printf("tasks[%d].total_block_num=%d\ttasks[%d].taskfilesz=%ld\n",
        i, read_total_block_num,i, read_task_filesz);
        printf("tasks[%d].task_memorysz=%ld\ttasks[%d].task_blockstart_offset=%ld\n\n",
        i, read_memory_filesz,i, read_task_blockstart_offset);
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
