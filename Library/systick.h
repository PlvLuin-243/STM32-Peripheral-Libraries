#ifndef SYSTICK_H
#define SYSTICK_H

#include "stm32f10x.h"

// Define system clock mac dinh
#define SYSTICK_CLOCK   72000000    // 8MHz

// Khai bao cac ham Timer cho Delay
void SysTick_Init(void);
void Delay_ms(uint32_t delay_ms);
void Delay_us(uint32_t delay_us);
uint32_t GetTick(void);
#endif