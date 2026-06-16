#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include <stdio.h>

void nonsecure_task(int n);

void task(void *argument)
{
    int n = 0;
    while (1) {
        nonsecure_task(++n);
        vTaskDelay(500 * configTICK_RATE_HZ / 1000);
    }
}

int main(void)
{
    *(volatile uint32_t *)0xE000E010 = 0; /* Systick を無効化（再起動時のため） */
    *(volatile uint32_t *)0xE000ED04 = 1 << 25; /* Systick がペンディングされていればクリア */
    __asm volatile ("cpsie f"); /* 割り込みを有効化 */
    xTaskCreate(task, "task", configMINIMAL_STACK_SIZE + 100, NULL, configMAX_PRIORITIES - 1, NULL);
    vTaskStartScheduler();
    return 0;
}
