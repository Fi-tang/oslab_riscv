#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

void Implement_Memcpy(void *dst, void *src, long size);
uint64_t load_task_img_first_alloc(int task_id);
uint64_t load_task_img_by_name(int tasknum, char *taskname);
long load_task_img_filesz(int tasknum, char *taskname);
long load_task_img_memorysz(int tasknum, char *taskname);

#endif