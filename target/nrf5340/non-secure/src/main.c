#include <zephyr.h>

#define STACKSIZE 1024
#define PRIORITY 7

void nonsecure_task(int n);

void task(void)
{
    int n = 0;
    while (1) {
        nonsecure_task(++n);
        k_msleep(500);
    }
}

K_THREAD_DEFINE(task_id, STACKSIZE, task, NULL, NULL, NULL, PRIORITY, 0, 0);
