#include "timer.h"
#include "stddef.h"

// =================== BIEN TOAN CUC ===================
// Bien cho delay mode
static TIM_TypeDef* g_delay_timer = NULL;
static uint32_t g_delay_tick_us = 0;

// Bien cho input capture TIM1
static uint32_t g_tim1_last_capture = 0;
static uint32_t g_tim1_current_capture = 0;
static uint32_t g_tim1_period = 0;
static uint8_t g_tim1_new_data = 0;
static uint8_t g_tim1_overflow_count = 0;
static uint16_t g_tim1_ppr = 0;  // Pulses Per Revolution
static uint8_t g_tim1_ic_enabled = 0;
static float g_tim1_rpm_buffer[200] = {0.0f};
static uint8_t g_tim1_rpm_buffer_index = 0;
static uint8_t g_tim1_rpm_sample_count = 0;

// =================== HAM TINH NANG CHUNG ===================
// Bat clock cho timer
static void Timer_Enable_Clock(TIM_TypeDef* TIMx)
{
    if(TIMx == TIM1)
        RCC->APB2ENR |= (1 << 11);      // TIM1: APB2 bus, bit 11
    else if(TIMx == TIM2)
        RCC->APB1ENR |= (1 << 0);       // TIM2: APB1 bus, bit 0
    else if(TIMx == TIM3)
        RCC->APB1ENR |= (1 << 1);       // TIM3: APB1 bus, bit 1
    else if(TIMx == TIM4)
        RCC->APB1ENR |= (1 << 2);       // TIM4: APB1 bus, bit 2
}

// Cau hinh co ban cho timer
static void Timer_Basic_Config(TIM_TypeDef* TIMx, uint16_t prescaler, uint16_t arr_value)
{
    TIMx->PSC = prescaler;              // Thiet lap bo chia tan so
    TIMx->ARR = arr_value;              // Thiet lap gia tri tu dong nap lai
    TIMx->CR1 = 0;                      // Reset thanh ghi dieu khien
    TIMx->CNT = 0;                      // Reset bo dem ve 0
    TIMx->EGR |= (1 << 0);              // Tao update event de load PSC va ARR
    TIMx->SR &= ~(1 << 0);              // Xoa co UIF sau khi load
    TIMx->DIER = 0;                     // Tat tat ca ngat
}

// =================== DELAY FUNCTIONS ===================
void Timer_Delay_Init(TIM_TypeDef* TIMx, uint16_t prescaler)
{
    // Kiem tra tham so dau vao
    if(TIMx == NULL) return;
    if(prescaler > 65535) return;
    
    // Bat clock cho timer
    Timer_Enable_Clock(TIMx);
    
    // Cau hinh co ban
    Timer_Basic_Config(TIMx, prescaler, 1000); // ARR tam thoi
    
    // Tinh toan thong so delay
    uint32_t timer_freq = SYSTEM_CLOCK / (prescaler + 1);
    g_delay_tick_us = 1000000 / timer_freq;
    g_delay_timer = TIMx;
    
    // Tat ngat cho che do delay
    if(TIMx == TIM1) NVIC_DisableIRQ(TIM1_UP_IRQn);
    else if(TIMx == TIM2) NVIC_DisableIRQ(TIM2_IRQn);
    else if(TIMx == TIM3) NVIC_DisableIRQ(TIM3_IRQn);
    else if(TIMx == TIM4) NVIC_DisableIRQ(TIM4_IRQn);
}
//  void Delay_us(uint16_t delay_us)
//  {
//      // Kiem tra timer delay da duoc init chua
//      if(g_delay_timer == NULL || g_delay_tick_us == 0) return;
    
//      // Tinh ARR value cho delay microsecond
//      uint32_t arr_value = (delay_us / g_delay_tick_us) - 1;
//      if(arr_value > 65535) return;
    
//      // Thuc hien delay
//      g_delay_timer->ARR = (uint16_t)arr_value;
//      g_delay_timer->CNT = 0;
//      g_delay_timer->SR &= ~(1 << 0);
//      g_delay_timer->CR1 |= (1 << 0);
    
//      while(!(g_delay_timer->SR & (1 << 0)));
    
//      g_delay_timer->CR1 &= ~(1 << 0);
//      g_delay_timer->SR &= ~(1 << 0);
//  }

//  void Delay_ms(uint16_t delay_ms)
//  {
//      // Kiem tra timer delay da duoc init chua
//      if(g_delay_timer == NULL || g_delay_tick_us == 0) return;
    
//      // Chuyen doi ms sang us va goi Delay_us
//      uint32_t delay_us = delay_ms * 1000;
    
//      // Chia nho neu delay qua lon (> 65535 us)
//      while(delay_us > 65535)
//      {
//          Delay_us(65535);
//          delay_us -= 65535;
//      }
    
//      // Delay phan con lai
//      if(delay_us > 0)
//      {
//          Delay_us((uint16_t)delay_us);
//      }
//  }
// =================== IRQ FUNCTIONS ===================
void Timer_IRQ_Init(TIM_TypeDef* TIMx, uint16_t prescaler, uint16_t arr_value)
{
    // Kiem tra tham so dau vao
    if(TIMx == NULL) return;
    if(prescaler > 65535 || arr_value > 65535) return;
    
    // Bat clock cho timer
    Timer_Enable_Clock(TIMx);
    
    // Cau hinh co ban
    Timer_Basic_Config(TIMx, prescaler, arr_value);
    
    // Bat ngat update
    TIMx->DIER |= (1 << 0);
    
    // Bat ngat NVIC
    if(TIMx == TIM1) NVIC_EnableIRQ(TIM1_UP_IRQn);
    else if(TIMx == TIM2) NVIC_EnableIRQ(TIM2_IRQn);
    else if(TIMx == TIM3) NVIC_EnableIRQ(TIM3_IRQn);
    else if(TIMx == TIM4) NVIC_EnableIRQ(TIM4_IRQn);
    
    // Khoi dong timer ngay
    TIMx->CR1 |= (1 << 0);
}

// =================== PWM FUNCTIONS ===================
// Cau hinh GPIO cho PWM
static void Timer_Config_GPIO_PWM(TIM_TypeDef* TIMx, uint8_t channel)
{
    if(TIMx == TIM1)
    {
        // Bat clock cho GPIOA
        RCC->APB2ENR |= (1 << 2);       // GPIOA clock enable
        
        if(channel == PWM_CHANNEL_1)
        {
            // PA8 - TIM1_CH1
            GPIOA->CRH &= ~(0xF << 0);      // Xoa cau hinh PA8
            GPIOA->CRH |= (0xB << 0);       // PA8: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_2)
        {
            // PA9 - TIM1_CH2
            GPIOA->CRH &= ~(0xF << 4);      // Xoa cau hinh PA9
            GPIOA->CRH |= (0xB << 4);       // PA9: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_3)
        {
            // PA10 - TIM1_CH3
            GPIOA->CRH &= ~(0xF << 8);      // Xoa cau hinh PA10
            GPIOA->CRH |= (0xB << 8);       // PA10: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_4)
        {
            // PA11 - TIM1_CH4
            GPIOA->CRH &= ~(0xF << 12);     // Xoa cau hinh PA11
            GPIOA->CRH |= (0xB << 12);      // PA11: Alternate function push-pull, 50MHz
        }
    }
    else if(TIMx == TIM2)
    {
        // Bat clock cho GPIOA
        RCC->APB2ENR |= (1 << 2);       // GPIOA clock enable
        
        if(channel == PWM_CHANNEL_1)
        {
            // PA0 - TIM2_CH1
            GPIOA->CRL &= ~(0xF << 0);      // Xoa cau hinh PA0
            GPIOA->CRL |= (0xB << 0);       // PA0: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_2)
        {
            // PA1 - TIM2_CH2
            GPIOA->CRL &= ~(0xF << 4);      // Xoa cau hinh PA1
            GPIOA->CRL |= (0xB << 4);       // PA1: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_3)
        {
            // PA2 - TIM2_CH3
            GPIOA->CRL &= ~(0xF << 8);      // Xoa cau hinh PA2
            GPIOA->CRL |= (0xB << 8);       // PA2: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_4)
        {
            // PA3 - TIM2_CH4
            GPIOA->CRL &= ~(0xF << 12);     // Xoa cau hinh PA3
            GPIOA->CRL |= (0xB << 12);      // PA3: Alternate function push-pull, 50MHz
        }
    }
    else if(TIMx == TIM3)
    {
        // Bat clock cho GPIOA va GPIOB
        RCC->APB2ENR |= (1 << 2);       // GPIOA clock enable
        RCC->APB2ENR |= (1 << 3);       // GPIOB clock enable
        
        if(channel == PWM_CHANNEL_1)
        {
            // PA6 - TIM3_CH1
            GPIOA->CRL &= ~(0xF << 24);     // Xoa cau hinh PA6
            GPIOA->CRL |= (0xB << 24);      // PA6: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_2)
        {
            // PA7 - TIM3_CH2
            GPIOA->CRL &= ~(0xF << 28);     // Xoa cau hinh PA7
            GPIOA->CRL |= (0xB << 28);      // PA7: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_3)
        {
            // PB0 - TIM3_CH3
            GPIOB->CRL &= ~(0xF << 0);      // Xoa cau hinh PB0
            GPIOB->CRL |= (0xB << 0);       // PB0: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_4)
        {
            // PB1 - TIM3_CH4
            GPIOB->CRL &= ~(0xF << 4);      // Xoa cau hinh PB1
            GPIOB->CRL |= (0xB << 4);       // PB1: Alternate function push-pull, 50MHz
        }
    }
    else if(TIMx == TIM4)
    {
        // Bat clock cho GPIOB
        RCC->APB2ENR |= (1 << 3);       // GPIOB clock enable
        
        if(channel == PWM_CHANNEL_1)
        {
            // PB6 - TIM4_CH1
            GPIOB->CRL &= ~(0xF << 24);     // Xoa cau hinh PB6
            GPIOB->CRL |= (0xB << 24);      // PB6: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_2)
        {
            // PB7 - TIM4_CH2
            GPIOB->CRL &= ~(0xF << 28);     // Xoa cau hinh PB7
            GPIOB->CRL |= (0xB << 28);      // PB7: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_3)
        {
            // PB8 - TIM4_CH3
            GPIOB->CRH &= ~(0xF << 0);      // Xoa cau hinh PB8
            GPIOB->CRH |= (0xB << 0);       // PB8: Alternate function push-pull, 50MHz
        }
        else if(channel == PWM_CHANNEL_4)
        {
            // PB9 - TIM4_CH4
            GPIOB->CRH &= ~(0xF << 4);      // Xoa cau hinh PB9
            GPIOB->CRH |= (0xB << 4);       // PB9: Alternate function push-pull, 50MHz
        }
    }
}

// Cau hinh PWM cho channel cu the
static void Timer_Config_PWM_Channel(TIM_TypeDef* TIMx, uint8_t channel, uint8_t duty_cycle)
{
    uint32_t ccr_value = (TIMx->ARR * duty_cycle) / 100;
    
    if(channel == PWM_CHANNEL_1)
    {
        // Cau hinh CCMR1 cho Channel 1
        TIMx->CCMR1 &= ~(7 << 4);       // Xoa OC1M
        TIMx->CCMR1 |= (6 << 4);        // OC1M = 110 (PWM mode 1)
        TIMx->CCMR1 |= (1 << 3);        // OC1PE = 1 (Preload enable)
        
        // Dat duty cycle
        TIMx->CCR1 = ccr_value;
        
        // Bat channel
        TIMx->CCER |= (1 << 0);         // CC1E = 1
    }
    else if(channel == PWM_CHANNEL_2)
    {
        // Cau hinh CCMR1 cho Channel 2
        TIMx->CCMR1 &= ~(7 << 12);      // Xoa OC2M
        TIMx->CCMR1 |= (6 << 12);       // OC2M = 110 (PWM mode 1)
        TIMx->CCMR1 |= (1 << 11);       // OC2PE = 1 (Preload enable)
        
        // Dat duty cycle
        TIMx->CCR2 = ccr_value;
        
        // Bat channel
        TIMx->CCER |= (1 << 4);         // CC2E = 1
    }
    else if(channel == PWM_CHANNEL_3)
    {
        // Cau hinh CCMR2 cho Channel 3
        TIMx->CCMR2 &= ~(7 << 4);       // Xoa OC3M
        TIMx->CCMR2 |= (6 << 4);        // OC3M = 110 (PWM mode 1)
        TIMx->CCMR2 |= (1 << 3);        // OC3PE = 1 (Preload enable)
        
        // Dat duty cycle
        TIMx->CCR3 = ccr_value;
        
        // Bat channel
        TIMx->CCER |= (1 << 8);         // CC3E = 1
    }
    else if(channel == PWM_CHANNEL_4)
    {
        // Cau hinh CCMR2 cho Channel 4
        TIMx->CCMR2 &= ~(7 << 12);      // Xoa OC4M
        TIMx->CCMR2 |= (6 << 12);       // OC4M = 110 (PWM mode 1)
        TIMx->CCMR2 |= (1 << 11);       // OC4PE = 1 (Preload enable)
        
        // Dat duty cycle
        TIMx->CCR4 = ccr_value;
        
        // Bat channel
        TIMx->CCER |= (1 << 12);        // CC4E = 1
    }
}

void Timer_PWM_Init(TIM_TypeDef* TIMx, uint8_t channel, uint16_t prescaler, uint16_t arr_value)
{
    // Kiem tra tham so
    if(TIMx == NULL) return;
    if(channel < PWM_CHANNEL_1 || channel > PWM_CHANNEL_4) return;
    if(arr_value == 0) return;

    // Bat clock
    Timer_Enable_Clock(TIMx);

    // Cau hinh timer co ban
    Timer_Basic_Config(TIMx, prescaler, arr_value);

    // Cau hinh GPIO
    Timer_Config_GPIO_PWM(TIMx, channel);

    // Cau hinh PWM channel, mac dinh duty = 0
    Timer_Config_PWM_Channel(TIMx, channel, 0);

    // Bat Auto-reload preload
    TIMx->CR1 |= (1 << 7);              // ARPE = 1

    // Doi voi TIM1 can enable main output
    if(TIMx == TIM1)
    {
        TIMx->BDTR |= (1 << 15);        // MOE = 1 (Main Output Enable)
    }

    // Tao update event
    TIMx->EGR |= (1 << 0);
    TIMx->SR &= ~(1 << 0);

    // Khoi dong timer
    TIMx->CR1 |= (1 << 0);
    
    // Dat duty cycle ban dau = 0
    Timer_PWM_Set_Duty_Cycle(TIMx, channel, 0);
}

void Timer_PWM_Set_Duty_Cycle(TIM_TypeDef* TIMx, uint8_t channel, uint8_t duty_cycle)
{
    if(TIMx == NULL) return;
    if(channel < PWM_CHANNEL_1 || channel > PWM_CHANNEL_4) return;
    if(duty_cycle > 100) return;
    
    uint32_t ccr_value = (TIMx->ARR * duty_cycle) / 100;
    
    switch(channel)
    {
        case PWM_CHANNEL_1:
            TIMx->CCR1 = ccr_value;
            break;
        case PWM_CHANNEL_2:
        
            TIMx->CCR2 = ccr_value;
            break;
        case PWM_CHANNEL_3:
            TIMx->CCR3 = ccr_value;
            break;
        case PWM_CHANNEL_4:
            TIMx->CCR4 = ccr_value;
            break;
    }
}

void Timer_PWM_Set_CCR_Value(TIM_TypeDef* TIMx, uint8_t channel, uint16_t ccr_value)
{
    if(TIMx == NULL) return;
    if(channel < PWM_CHANNEL_1 || channel > PWM_CHANNEL_4) return;
    
    // Gioi han CCR value khong vuot qua ARR
    if(ccr_value > TIMx->ARR) ccr_value = TIMx->ARR;
    
    switch(channel)
    {
        case PWM_CHANNEL_1:
            TIMx->CCR1 = ccr_value;
            break;
        case PWM_CHANNEL_2:
            TIMx->CCR2 = ccr_value;
            break;
        case PWM_CHANNEL_3:
            TIMx->CCR3 = ccr_value;
            break;
        case PWM_CHANNEL_4:
            TIMx->CCR4 = ccr_value;
            break;
    }
}

void Timer_PWM_Set_Frequency(TIM_TypeDef* TIMx, uint16_t frequency_hz)
{
    if(TIMx == NULL || frequency_hz == 0) return;
    
    // Tinh lai ARR va PSC
    uint32_t timer_clock = SYSTEM_CLOCK;
    uint32_t period = timer_clock / frequency_hz;
    uint16_t prescaler = 0;
    uint16_t arr = period - 1;
    
    while(arr > 65535)
    {
        prescaler++;
        arr = (period / (prescaler + 1)) - 1;
    }
    
    // Cap nhat PSC va ARR
    TIMx->PSC = prescaler;
    TIMx->ARR = arr;
    
    // Tao update event
    TIMx->EGR |= (1 << 0);
    TIMx->SR &= ~(1 << 0);
}

void Timer_PWM_Enable(TIM_TypeDef* TIMx, uint8_t channel)
{
    if(TIMx == NULL) return;
    if(channel < PWM_CHANNEL_1 || channel > PWM_CHANNEL_4) return;
    
    switch(channel)
    {
        case PWM_CHANNEL_1:
            TIMx->CCER |= (1 << 0);     // CC1E = 1
            break;
        case PWM_CHANNEL_2:
            TIMx->CCER |= (1 << 4);     // CC2E = 1
            break;
        case PWM_CHANNEL_3:
            TIMx->CCER |= (1 << 8);     // CC3E = 1
            break;
        case PWM_CHANNEL_4:
            TIMx->CCER |= (1 << 12);    // CC4E = 1
            break;
    }

    if(!(TIMx->CR1 & (1 << 0)))
    {
        TIMx->CR1 |= (1 << 0);          // Bat timer neu chua chay
    }
}

void Timer_PWM_Disable(TIM_TypeDef* TIMx, uint8_t channel)
{
    if(TIMx == NULL) return;
    if(channel < PWM_CHANNEL_1 || channel > PWM_CHANNEL_4) return;
    
    switch(channel)
    {
        case PWM_CHANNEL_1:
            TIMx->CCER &= ~(1 << 0);    // CC1E = 0
            break;
        case PWM_CHANNEL_2:
            TIMx->CCER &= ~(1 << 4);    // CC2E = 0
            break;
        case PWM_CHANNEL_3:
            TIMx->CCER &= ~(1 << 8);    // CC3E = 0
            break;
        case PWM_CHANNEL_4:
            TIMx->CCER &= ~(1 << 12);   // CC4E = 0
            break;
    }
}

// =================== INPUT CAPTURE FUNCTIONS ===================
void TIM1_Input_Capture_Init(uint16_t pulses_per_revolution)
{
    // Luu so xung tren 1 vong
    g_tim1_ppr = pulses_per_revolution;
    g_tim1_ic_enabled = 1;
    
    // Bat clock cho TIM1 va GPIOA
    RCC->APB2ENR |= (1 << 11);      // TIM1 clock enable
    RCC->APB2ENR |= (1 << 2);       // GPIOA clock enable
    
    // Cau hinh PA8 lam input pull-down cho TIM1_CH1
    GPIOA->CRH &= ~(0xF << 0);      // Xoa cau hinh PA8
    GPIOA->CRH |= (0x8 << 0);       // PA8: Input pull-up/pull-down
    GPIOA->ODR &= ~(1 << 8);        // Dat pull-down cho PA8
    
    // Reset TIM1
    TIM1->CR1 = 0;
    TIM1->CR2 = 0;
    TIM1->SMCR = 0;
    TIM1->DIER = 0;
    TIM1->SR = 0;
    TIM1->CNT = 0;
    
    // Cau hinh prescaler: 72MHz / (71+1) = 1MHz = 1us per tick
    TIM1->PSC = 71;
    TIM1->ARR = 0xFFFF;             // Max period
    
    // Cau hinh Channel 1 cho Input Capture
    TIM1->CCMR1 &= ~(3 << 0);       // Xoa CC1S
    TIM1->CCMR1 |= (1 << 0);        // CC1S = 01 (IC1 mapped on TI1)
    TIM1->CCMR1 &= ~(0xF << 4);     // Xoa IC1F (khong loc)
    TIM1->CCMR1 &= ~(3 << 2);       // Xoa IC1PSC (khong chia)

    // Cau hinh edge detection (rising edge)
    TIM1->CCER &= ~(1 << 1);        // CC1P = 0 (rising edge)
    TIM1->CCER |= (1 << 0);         // CC1E = 1 (enable capture)
    
    // Bat ngat
    TIM1->DIER |= (1 << 1);         // CC1IE = 1 (Capture interrupt)
    TIM1->DIER |= (1 << 0);         // UIE = 1 (Update interrupt)
    
    // Bat ngat NVIC
    NVIC_EnableIRQ(TIM1_CC_IRQn);   // Capture interrupt
    NVIC_EnableIRQ(TIM1_UP_IRQn);   // Update interrupt
    
    // Reset cac bien
    g_tim1_last_capture = 0;
    g_tim1_current_capture = 0;
    g_tim1_period = 0;
    g_tim1_new_data = 0;
    g_tim1_overflow_count = 0;
    
    // Khoi dong timer
    TIM1->CR1 |= (1 << 0);
}

void TIM1_Input_Capture_IRQ_Handler(void)
{
    if(!g_tim1_ic_enabled) return;
    
    // Kiem tra ngat overflow
    if(TIM1->SR & (1 << 0))  // UIF
    {
        TIM1->SR &= ~(1 << 0);  // Xoa UIF
        g_tim1_overflow_count++;
        
        // Neu qua nhieu overflow thi reset
        if(g_tim1_overflow_count > 50)
        {
            g_tim1_last_capture = 0;
            g_tim1_overflow_count = 0;
        }
    }
    
    // Kiem tra ngat capture Channel 1
    if(TIM1->SR & (1 << 1))  // CC1IF
    {
        TIM1->SR &= ~(1 << 1);  // Xoa CC1IF
        
        g_tim1_current_capture = TIM1->CCR1;
        
        if(g_tim1_last_capture != 0)
        {
            // Tinh chu ky (xu ly overflow)
            if(g_tim1_overflow_count > 0)
            {
                // Co overflow xay ra
                g_tim1_period = (g_tim1_overflow_count * 65536) + 
                               g_tim1_current_capture - g_tim1_last_capture;
            }
            else if(g_tim1_current_capture >= g_tim1_last_capture)
            {
                g_tim1_period = g_tim1_current_capture - g_tim1_last_capture;
            }
            else
            {
                // Overflow 1 lan
                g_tim1_period = (65536 - g_tim1_last_capture) + g_tim1_current_capture;
            }
            g_tim1_new_data = 1;
        }
        
        g_tim1_last_capture = g_tim1_current_capture;
        g_tim1_overflow_count = 0;  // Reset overflow count
    }
}

float TIM1_Get_RPM(void)
{
    if(!g_tim1_ic_enabled || !g_tim1_new_data || 
       g_tim1_period == 0 || g_tim1_ppr == 0) return 0.0f;
    
    // Chu ky tinh bang microsecond (timer tick = 1us)
    uint32_t period_us = g_tim1_period;
    
    // Tinh RPM: RPM = (60 * 1000000us) / (period_us * pulses_per_revolution)
    float rpm = 60000000.0f / (period_us * g_tim1_ppr);
    
    g_tim1_new_data = 0;  // Xoa flag
    return rpm;
}

float TIM1_Get_RPM_Average(void)
{
    if(!g_tim1_ic_enabled || !g_tim1_new_data || 
       g_tim1_period == 0 || g_tim1_ppr == 0) return 0.0f;
    
    // Lay gia tri RPM tuc thoi
    float rpm_instant = TIM1_Get_RPM();
    
    // Luu vao buffer
    g_tim1_rpm_buffer[g_tim1_rpm_buffer_index] = rpm_instant;
    g_tim1_rpm_buffer_index = (g_tim1_rpm_buffer_index + 1) % 200; // Vong quanh buffer
    
    // Tang so mau
    if(g_tim1_rpm_sample_count < 200)
    {
        g_tim1_rpm_sample_count++;
    }
    
    // Tinh trung binh
    float rpm_sum = 0.0f;
    for(uint8_t i = 0; i < g_tim1_rpm_sample_count; i++)
    {
        rpm_sum += g_tim1_rpm_buffer[i];
    }
    
    return rpm_sum / g_tim1_rpm_sample_count;
}

void TIM1_Reset_RPM_Buffer(void)
{
    for(uint8_t i = 0; i < 200; i++)
    {
        g_tim1_rpm_buffer[i] = 0.0f;
    }
    g_tim1_rpm_buffer_index = 0;
    g_tim1_rpm_sample_count = 0;
}

uint8_t TIM1_Is_RPM_Ready(void)
{
    if(!g_tim1_ic_enabled) return 0;
    return g_tim1_new_data;
}

float TIM1_Get_Frequency_Hz(void)
{
    if(!g_tim1_ic_enabled || !g_tim1_new_data || g_tim1_period == 0) return 0.0f;
    
    float frequency = 1000000.0f / g_tim1_period;  // Hz
    g_tim1_new_data = 0;
    return frequency;
}

// =================== COMMON FUNCTIONS ===================
void Timer_Stop(TIM_TypeDef* TIMx)
{
    if(TIMx == NULL) return;
    TIMx->CR1 &= ~(1 << 0);
}

void Timer_Start(TIM_TypeDef* TIMx)
{
    if(TIMx == NULL) return;
    TIMx->CR1 |= (1 << 0);
}

// =================== CALLBACK FUNCTIONS ===================
__attribute__((weak)) void Timer1_IRQ_Callback(void) { }
__attribute__((weak)) void Timer2_IRQ_Callback(void) { }
__attribute__((weak)) void Timer3_IRQ_Callback(void) { }
__attribute__((weak)) void Timer4_IRQ_Callback(void) { }

// =================== INTERRUPT HANDLERS ===================
void TIM1_CC_IRQHandler(void)
{
    TIM1_Input_Capture_IRQ_Handler();
}

void TIM1_UP_IRQHandler(void)
{
    // Xu ly Input Capture overflow truoc
    if(g_tim1_ic_enabled)
    {
        TIM1_Input_Capture_IRQ_Handler();
    }
    
    if(TIM1->SR & (1 << 0))
    {
        TIM1->SR &= ~(1 << 0);
        Timer1_IRQ_Callback();
    }
}

void TIM2_IRQHandler(void)
{
    if(TIM2->SR & (1 << 0))
    {
        TIM2->SR &= ~(1 << 0);
        Timer2_IRQ_Callback();
    }
}

void TIM3_IRQHandler(void)
{
    if(TIM3->SR & (1 << 0))
    {
        TIM3->SR &= ~(1 << 0);
        Timer3_IRQ_Callback();
    }
}

void TIM4_IRQHandler(void)
{
    if(TIM4->SR & (1 << 0))
    {
        TIM4->SR &= ~(1 << 0);
        Timer4_IRQ_Callback();
    }
}