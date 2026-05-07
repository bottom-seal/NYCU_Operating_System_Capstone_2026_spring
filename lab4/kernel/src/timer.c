#include "timer.h"
#include "uart.h"
#include "sbi.h"
#include "str.h"
#include "task.h"
#include "page_alloc.h"

static uint64_t boot_time = 0;
static struct list_head timer_list;

inline uint64_t rdtime(void) {
    uint64_t t;
    //calls assembly rdtime to get tick numbers
    asm volatile("rdtime %0" : "=r"(t));
    return t;
}

//we keep the earliest deadline at the front of the list
static void program_next_timer(void) {
    if (list_empty(&timer_list)) {
        sbi_set_timer_legacy((uint64_t)-1);//need to keep a timer to avoid weird things happening
        return;
    }

    struct timer_event *first =
        list_entry(timer_list.next, struct timer_event, list);//given the attribute list, get the struct timer_event that contains it

    sbi_set_timer_legacy(first->expire_time);
}

void timer_init(void) {
    boot_time = rdtime();
    INIT_LIST_HEAD(&timer_list);
    //sie : Supervisor Interrupt Enable
    //stores which kinds of supervisor-level interrupts are allowed, bitmask.
    asm volatile("csrs sie, %0" :: "r"(1UL << 5));      // bit 5 is STIE, Supervisor Timer Interrupt Enable
    //asm volatile("csrs sstatus, %0" :: "r"(1UL << 1));  // bit 1 SIE, global supervisor interrupts, decided to move it out
}

int add_timer(timer_callback_t cb, void *arg, int sec) {
    struct timer_event *new_ev;
    struct list_head *curr;//walk throught the list
    int inserted = 0;

    if (sec < 0)//check input legal
        return -1;

    new_ev = (struct timer_event *)allocate(sizeof(struct timer_event));//allocate for event struct
    if (new_ev == 0) {
        uart_puts("timer alloc failed\n");
        return -1;
    }

    new_ev->expire_time = rdtime() + (uint64_t)sec * cpu_freq;//get absolute tick
    new_ev->callback = cb;
    new_ev->arg = arg;
    INIT_LIST_HEAD(&new_ev->list);

    for (curr = timer_list.next; curr != &timer_list; curr = curr->next) {//when back to list head, means had run a full loop
        struct timer_event *entry =//get the struct containing list node
            list_entry(curr, struct timer_event, list);

        if (new_ev->expire_time < entry->expire_time) {//new timer has earlier deadline?
            // insert before curr
            list_add_tail(&new_ev->list, curr);//curr now is new_ev's tail
            inserted = 1;
            break;
        }
    }

    if (!inserted) {//the case if new timer event is the last deadline
        list_add_tail(&new_ev->list, &timer_list);//head is not new_ev's tail, meaning it's the last element
    }

    program_next_timer();//check if the front updated, and set timer
    return 0;
}

//Wrapper for setTImeout command
int add_timeout_message(const char *msg, int sec) {
    struct timer_event *new_ev;
    char *msg_copy;//node would be popped before deferred task execute, too to allocate memory and save somewhere
    struct list_head *curr;//walk through the list
    int inserted = 0;

    //error handling
    if (sec < 0)//check input legal
        return -1;

    if (strlen(msg) >= TIMER_MSG_LEN) {//64
        uart_puts("message too long\n");
        return -1;
    }

    msg_copy = (char *)allocate(strlen(msg) + 1);//need to allocate memory for message, this was made because the printing is deferred
    if (msg_copy == 0) {
        uart_puts("timer message alloc failed\n");
        return -1;
    }

    new_ev = (struct timer_event *)allocate(sizeof(struct timer_event));
    if (new_ev == 0) {
        free(msg_copy);
        uart_puts("timer alloc failed\n");
        return -1;
    }

    //same as add_timer, but copy the message to allocated memory, args points to it
    new_ev->expire_time = rdtime() + (uint64_t)sec * cpu_freq;//get absolute time
    new_ev->callback = timeout_callback;//this function is a wrapper for setTimeout only
    strcpy(msg_copy, msg);
    new_ev->arg = msg_copy;
    INIT_LIST_HEAD(&new_ev->list);
    //same loop to add to front of curr
    for (curr = timer_list.next; curr != &timer_list; curr = curr->next) {
        struct timer_event *entry =
            list_entry(curr, struct timer_event, list);

        if (new_ev->expire_time < entry->expire_time) {
            // insert before curr
            list_add_tail(&new_ev->list, curr);
            inserted = 1;
            break;
        }
    }
    //on no found, add this list tail
    if (!inserted) {
        list_add_tail(&new_ev->list, &timer_list);
    }
    //check if front is modified, run settimer
    program_next_timer();
    return 0;
}

void timeout_callback(void *arg) {
    char *msg = (char *)arg;//pointer msg points to the allocated memory for msg_copy in add_timeout_message function
    uart_puts(msg);
    uart_puts("\n");
    free(msg);//frees the allocated memory
}

void tick_callback(void *arg) {
    //tick logic takes current time of the deferred execution, if timer priority low, we could get weird number (not multiple of 2)
    uint64_t now = rdtime();//get current tick
    uint64_t sec = (now - boot_time) / cpu_freq;//get the CPU time in second 

    uart_puts("boot time: ");
    uart_int(sec);
    uart_puts("\n");

    add_timer(tick_callback, 0, 2);//add another timer for next tick in 2 second
}

void timer_interrupt_handler(void) {
    uint64_t now = rdtime();
    //check whole list, find all expired timers
    while (!list_empty(&timer_list)) {
        struct timer_event *first =//first timer in the list, assume we order by expiring time
            list_entry(timer_list.next, struct timer_event, list);

        if (first->expire_time > now)//if current one has not expired, later ones are all longer, break
            break;
        //if first element has expired, go this branch
        timer_callback_t cb = first->callback;
        void *arg = first->arg;
        //remove first element
        list_del(&first->list);
        //should always be valid cb for the 2 timer event case
        if (cb)
            add_task(cb, arg, TASK_PRIO_TIMER);

        free(first);//free the timer event struct
    }

    program_next_timer();//modified the list, need to recheck next interrupt time
}
