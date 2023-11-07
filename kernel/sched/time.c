#include <os/list.h>
#include <os/sched.h>
#include <type.h>

volatile uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    list_head *sleep_node = (&sleep_queue) -> next;
    while(sleep_node != (&sleep_queue)){
        list_node_t *sleep_node_next = sleep_node -> next;
        pcb_t *sleep_pcb = GetPcb_FromList(sleep_node);
        if(sleep_pcb -> wakeup_time == 0){
            DequeNode_AccordList(&sleep_queue, sleep_node);
            do_unblock(sleep_node);
        }
        else{
            sleep_pcb -> wakeup_time -= 1;
        }
        sleep_node = sleep_node_next;
    }
}