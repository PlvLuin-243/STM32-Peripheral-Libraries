#ifndef TIMER_H
#define TIMER_H

#include "stm32f10x.h"

// Define system clock mac dinh
#define SYSTEM_CLOCK    8000000    // 72MHz

// Define PWM channels
#define PWM_CHANNEL_1       1
#define PWM_CHANNEL_2       2
#define PWM_CHANNEL_3       3
#define PWM_CHANNEL_4       4

// Khai bao cac ham Timer cho Delay
void SysTick_Init(void);
void Timer_Delay_Init(TIM_TypeDef* TIMx, uint16_t prescaler);
void Delay_ms(uint32_t delay_ms);
void Delay_us(uint32_t delay_us);

// Khai bao cac ham Timer cho IRQ
void Timer_IRQ_Init(TIM_TypeDef* TIMx, uint16_t prescaler, uint16_t arr_value);

// Khai bao cac ham Timer cho PWM
void Timer_PWM_Init(TIM_TypeDef* TIMx, uint8_t channel, uint16_t prescaler, uint16_t arr_value);
void Timer_PWM_Set_Duty_Cycle(TIM_TypeDef* TIMx, uint8_t channel, uint8_t duty_cycle);
void Timer_PWM_Set_CCR_Value(TIM_TypeDef* TIMx, uint8_t channel, uint16_t ccr_value);
void Timer_PWM_Set_Frequency(TIM_TypeDef* TIMx, uint16_t frequency_hz);
void Timer_PWM_Enable(TIM_TypeDef* TIMx, uint8_t channel);
void Timer_PWM_Disable(TIM_TypeDef* TIMx, uint8_t channel);

// Khai bao cac ham Timer cho Input Capture
void TIM1_Input_Capture_Init(uint16_t pulses_per_revolution);
void TIM1_Input_Capture_IRQ_Handler(void);
float TIM1_Get_RPM(void);
float TIM1_Get_RPM_Average(void);

// Cac ham chung
void Timer_Stop(TIM_TypeDef* TIMx);
void Timer_Start(TIM_TypeDef* TIMx);

// Weak callback functions 
__attribute__((weak))  void Timer1_IRQ_Callback(void);
__attribute__((weak))  void Timer2_IRQ_Callback(void);
__attribute__((weak))  void Timer3_IRQ_Callback(void);
__attribute__((weak))  void Timer4_IRQ_Callback(void);

#endif