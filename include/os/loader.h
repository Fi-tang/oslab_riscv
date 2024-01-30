#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <type.h>
#include <os/task.h> // newly added!
#include <os/mm.h>

uint64_t load_task_img_first_alloc(int task_id);
uint64_t load_task_img_by_name(int tasknum, char *taskname);

//*******************virtual address**********************
void load_task_image(char *taskname, PTE *user_level_one_pgdir);
void Build_user_page_table(int task_id, PTE *user_level_one_pgdir, uintptr_t *task_page_array);
#endif