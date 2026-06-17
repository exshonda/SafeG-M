/*
 *  Non-secure 側 最小スタートアップ（Raspberry Pi Pico2 / RP2350, SafeG-M FreeRTOS）
 *
 *  Secure 側 launch_ns が NS ベクタ先頭 [0]=初期MSP, [1]=ResetISR を読んで
 *  NS(FreeRTOS) を起動する。ResetISR は .data コピー・.bss ゼロクリア後 main() を呼ぶ。
 *  例外/割込みは Secure 側がマスク制御するため最小のハンドラのみ。ただし
 *  FreeRTOS(ARM_CM33_NTZ port)が用いる SVC/PendSV/SysTick はベクタに配線する
 *  （FreeRTOSConfig.h で vPortSVCHandler=SVC_Handler 等に #define 済み）。
 */
#include <stdint.h>

extern int  main(void);
extern uint32_t __data_load, __data_start, __data_end, __bss_start, __bss_end;
extern uint32_t __stack_top;

/* FreeRTOS port(ARM_CM33_NTZ/non_secure)が提供する例外ハンドラ */
extern void SVC_Handler(void);
extern void PendSV_Handler(void);
extern void SysTick_Handler(void);

/* FreeRTOSConfig.h が extern 参照する（本テストでは値は未使用だが定義しておく） */
uint32_t SystemCoreClock = 150000000u;

void ResetISR(void);
static void Default_Handler(void);

/* NS ベクタテーブル。先頭2語(MSP/Reset)を launch_ns が使用し、
 * 以降は VTOR_NS 経由で例外ディスパッチされる。 */
__attribute__((used, section(".isr_vector")))
void (* const g_ns_vectors[])(void) = {
    (void (*)(void))(&__stack_top),   /*  0: 初期 MSP       */
    ResetISR,                          /*  1: Reset          */
    Default_Handler,                   /*  2: NMI            */
    Default_Handler,                   /*  3: HardFault      */
    Default_Handler,                   /*  4: MemManage      */
    Default_Handler,                   /*  5: BusFault       */
    Default_Handler,                   /*  6: UsageFault     */
    Default_Handler,                   /*  7: SecureFault    */
    0, 0, 0,                           /*  8-10: 予約        */
    SVC_Handler,                       /* 11: SVCall (FreeRTOS) */
    Default_Handler,                   /* 12: DebugMon       */
    0,                                 /* 13: 予約           */
    PendSV_Handler,                    /* 14: PendSV (FreeRTOS) */
    SysTick_Handler,                   /* 15: SysTick (FreeRTOS) */
};

void ResetISR(void)
{
    uint32_t *src = &__data_load;
    uint32_t *dst = &__data_start;
    while (dst < &__data_end) {
        *dst++ = *src++;
    }
    for (dst = &__bss_start; dst < &__bss_end; ) {
        *dst++ = 0u;
    }
    (void)main();
    for (;;) {
        /* main は戻らない想定 */
    }
}

static void Default_Handler(void)
{
    for (;;) {
    }
}
