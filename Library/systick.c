#include "systick.h"

static volatile uint32_t g_systick_counter = 0;

void SysTick_Init(void)
{
    // Cau hinh SysTick cho 1ms interrupt
    SysTick->LOAD = (SYSTICK_CLOCK / 1000) - 1; // 72MHz / 1000 = 72kHz = 1ms
    SysTick->VAL = 0;
    SysTick->CTRL = 0x07; // Enable SysTick + interrupt + processor clock
}
void Delay_us(uint32_t delay_us)
{
    // Backup cau hinh SysTick
    uint32_t ctrl_backup = SysTick->CTRL;
    
    // Cau hinh cho microsecond: 72MHz / 72 = 1MHz = 1us
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk; // Tat ngat
    SysTick->LOAD = delay_us * (SYSTICK_CLOCK / 1000000) - 1;
    SysTick->VAL = 0;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

    // Cho cho den khi het thoi gian
    while(!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk));

    // Khoi phuc cau hinh cu
    SysTick->CTRL = ctrl_backup;
    SysTick->LOAD = (SYSTICK_CLOCK / 1000) - 1;
    SysTick->VAL = 0;
}

void Delay_ms(uint32_t delay_ms)
{
    // Su dung bo dem SysTick counter de delay millisecond
    uint32_t start = g_systick_counter;
    while((g_systick_counter - start) < delay_ms);
}
uint32_t GetTick(void)
{
    return g_systick_counter;
}
// Ham xu ly ngat SysTick
void SysTick_Handler(void)
{
    g_systick_counter++;
}