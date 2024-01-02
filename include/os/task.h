#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      48
#define TASK_SIZE        0x10000

#define EI_NIDENT   (16)
#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    char taskname[EI_NIDENT];
    int start_block_id;
    int total_block_num;
    long task_filesz;
    long task_memorysz;
    long task_blockstart_offset;
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];

#endif