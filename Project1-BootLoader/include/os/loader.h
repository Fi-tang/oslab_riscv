#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>

uint64_t load_task_img(int taskid);
long load_taskfilesz(int tasknum, char *taskname);
long load_taskmemorysz(int tasknum, char *taskname);
uint64_t load_task_img_by_name(int tasknum, char *taskname);

#endif