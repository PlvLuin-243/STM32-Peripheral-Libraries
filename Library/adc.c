//===============================================//
//              ADC Library Implementation
//===============================================//
#include "adc.h"
#include "stm32f10x.h"

//================================================================================================//
//                             KHAI BÁO BIẾN TOÀN CỤC
//================================================================================================//

// Bien ADC DMA
volatile uint16_t adc_value = 0;       // Gia tri ADC tu DMA
volatile uint8_t adc_update_flag = 0;  // Co cap nhat ADC

// Bien PWM (extern - se duoc dinh nghia trong file chinh)
//extern volatile int Duty_Cycle;
//extern volatile int motor_enabled;

//================================================================================================//
//                          CẤU HÌNH PHẦN CỨNG (HARDWARE CONFIG)
//================================================================================================//

//------------------------------------------------------------------------------------------------//
//                          DMA Channel 1 Configuration (ADC)
//------------------------------------------------------------------------------------------------//
void DMA1_Channel1_Config(void)
{
    RCC->AHBENR |= (1 << 0);  // DMA1 clock
    
    DMA1_Channel1->CCR &= ~(1 << 0);  // Disable
    
    // Addresses
    DMA1_Channel1->CPAR = (uint32_t)(&(ADC1->DR));
    DMA1_Channel1->CMAR = (uint32_t)(&adc_value);
    DMA1_Channel1->CNDTR = 1;
    
    // Data size
    DMA1_Channel1->CCR &= ~(0b11 << 8);
    DMA1_Channel1->CCR |=  (1 << 8);     // PSIZE 16-bit
    DMA1_Channel1->CCR &= ~(0b11 << 10);
    DMA1_Channel1->CCR |=  (1 << 10);    // MSIZE 16-bit
    
    // Mode
    DMA1_Channel1->CCR &= ~((1 << 7) | (1 << 6));  // No increment
    DMA1_Channel1->CCR |=  (1 << 5);               // Circular
    DMA1_Channel1->CCR &= ~(1 << 4);               // Peripheral to Memory
    
    DMA1_Channel1->CCR |=  (1 << 0);   // Enable
}

//------------------------------------------------------------------------------------------------//
//                                ADC1 Configuration with DMA
//------------------------------------------------------------------------------------------------//
void ADC1_DMA_Config(uint8_t channel)
{
    RCC->APB2ENR |= (1 << 9);  // ADC1 clock
    
    // Channel selection (flexible)
    ADC1->SQR1 &= ~(0xF << 20);       // Clear sequence length
    ADC1->SQR3 &= ~(0x1F << 0);       // Clear channel selection
    ADC1->SQR3 |= (channel << 0);     // Set channel (0=PA0, 1=PA1, 2=PA2, ...)
    
    // Sample time (channel dependent)
    if (channel < 10) {
        // Channels 0-9: Use SMPR2
        ADC1->SMPR2 &= ~(0x7 << (3 * channel));
        ADC1->SMPR2 |=  (7 << (3 * channel));  // 239.5 cycles
    } else {
        // Channels 10-17: Use SMPR1
        ADC1->SMPR1 &= ~(0x7 << (3 * (channel - 10)));
        ADC1->SMPR1 |=  (7 << (3 * (channel - 10)));  // 239.5 cycles
    }
    
    // Modes
    ADC1->CR1 |= (1 << 8);     // SCAN mode
    ADC1->CR2 |= (1 << 8);     // DMA mode
    ADC1->CR2 |= (1 << 1);     // Continuous
    
    // Power on & Calibration
    ADC1->CR2 |=  (1 << 0);    // ADON
    // Delay required here (use external delay function)
    // delay_ms(1);
    ADC1->CR2 |=  (1 << 2);    // CAL
    while (ADC1->CR2 & (1 << 2));
    
    // Start conversion
    ADC1->CR2 |= (1 << 0);
}

//================================================================================================//
//                              XỬ LÝ LOGIC (APPLICATION LOGIC)
//================================================================================================//

//------------------------------------------------------------------------------------------------//
//                                   Process ADC Value
//------------------------------------------------------------------------------------------------//
//void Process_ADC(void)
//{
//    uint16_t adc_val = adc_value;
//    
//    // Convert ADC (0-4095) to Duty_Cycle (0-179)
//    uint32_t temp = ((uint32_t)adc_val * 180) / 4095;
//    Duty_Cycle = (int)temp;
//    
//    // Limit to 179
//    if(Duty_Cycle > 180) {
//        Duty_Cycle = 179;
//    }
//    if(Duty_Cycle < 0) {
//        Duty_Cycle = 0;
//    }
//    
//    // Update PWM if motor is ON
//    // Note: TIM1 register access requires extern TIM1 or pass as parameter
//    if(motor_enabled) {
//        TIM1->CCR1 = TIM1->ARR - Duty_Cycle;
//    }
//}
