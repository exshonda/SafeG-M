/*
 *  Non-secure 側 最小スタートアップ（ARM MPS2-AN505 / QEMU 用, SafeG-M テスト）
 *
 *  launch_ns が ベクタ[0]=初期MSP, [1]=ResetISR を読んで NS を起動する．
 *  ResetISR は .data コピー・.bss ゼロクリア後 main() を呼ぶ．
 *  例外/割込みは Secure 側がマスク制御するため，最小のハンドラのみ用意する．
 */
#include <stdint.h>

extern int  main(void);
extern uint32_t __data_load, __data_start, __data_end, __bss_start, __bss_end;
extern uint32_t __stack_top;

void ResetISR(void);
static void Default_Handler(void);

/* NS ベクタテーブル（先頭2語のみ launch_ns が使用） */
__attribute__((used, section(".ns_vector")))
void (* const g_ns_vectors[])(void) = {
    (void (*)(void))(&__stack_top),   /* [0] 初期 MSP            */
    ResetISR,                          /* [1] Reset               */
    Default_Handler,                   /* [2] NMI                 */
    Default_Handler,                   /* [3] HardFault           */
    Default_Handler,                   /* [4] MemManage           */
    Default_Handler,                   /* [5] BusFault            */
    Default_Handler,                   /* [6] UsageFault          */
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
