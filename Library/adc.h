//===============================================//
//              ADC Library Header
//===============================================//
#ifndef __ADC_H
#define __ADC_H

#include "stm32f10x.h"

//================================================================================================//
//                             KHAI BÁO BIẾN TOÀN CỤC
//================================================================================================//

// Bien ADC DMA
extern volatile uint16_t adc_value;       // Gia tri ADC tu DMA
extern volatile uint8_t adc_update_flag;  // Co cap nhat ADC

//================================================================================================//
//                             KHAI BÁO PROTOTYPE CÁC HÀM
//================================================================================================//

// Cau hinh phan cung
void DMA1_Channel1_Config(void);     // Cau hinh DMA Channel 1 cho ADC
void ADC1_DMA_Config(uint8_t channel);  // Cau hinh ADC voi DMA cho channel cu the (0=PA0, 1=PA1, ...)
void Process_ADC(void);              // Xu ly gia tri ADC

#endif // __ADC_H
