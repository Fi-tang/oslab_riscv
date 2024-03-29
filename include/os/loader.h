#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

uint64_t load_task_img_by_name(int tasknum, char *taskname);
long load_task_img_filesz(int tasknum, char *taskname);
long load_task_img_memorysz(int tasknum, char *taskname);

#endif