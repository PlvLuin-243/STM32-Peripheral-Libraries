#include "gpio.h"

/*
 * ===========================================================================
 * THU VIEN GPIO CHO STM32F1 
 * ===========================================================================
 */

// Ham ho tro bat clock cho GPIO port
void gpio_enable_clock(GPIO_TypeDef* GPIOx)
{
    if(GPIOx == GPIOA)
    {
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;         // Bat clock cho GPIOA
    }
    else if(GPIOx == GPIOB)
    {
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;         // Bat clock cho GPIOB
    }
    else if(GPIOx == GPIOC)
    {
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;         // Bat clock cho GPIOC
    }
    else if(GPIOx == GPIOD)
    {
        RCC->APB2ENR |= RCC_APB2ENR_IOPDEN;         // Bat clock cho GPIOD
    }
    else if(GPIOx == GPIOE)
    {
        RCC->APB2ENR |= RCC_APB2ENR_IOPEEN;         // Bat clock cho GPIOE
    }
}

/**
 * @brief Khoi tao GPIO pin lam output
 */
void gpio_init_output(GPIO_TypeDef* GPIOx, uint16_t pin)
{
    // BUOC 1: BAT CLOCK cho GPIO Port
    gpio_enable_clock(GPIOx);
    
    // BUOC 2: CAU HINH PIN LAM OUTPUT PUSH-PULL 2MHz
    if(pin < 8)                                     // Pin 0-7 su dung thanh ghi CRL 
    {
        GPIOx->CRL &= ~(0xF << (pin * 4));          // Xoa 4 bit cau hinh cu
        GPIOx->CRL |= (0x2 << (pin * 4));           // Output 2MHz Push-pull (0010)
    }
    else if(pin < 16)                               // Pin 8-15 su dung thanh ghi CRH 
    {
        uint8_t pin_offset = pin - 8;               // Tinh vi tri pin trong CRH (0-7)
        GPIOx->CRH &= ~(0xF << (pin_offset * 4));   // Xoa 4 bit cau hinh cu
        GPIOx->CRH |= (0x2 << (pin_offset * 4));    // Output 2MHz Push-pull
    }
}

/**
 * @brief Khoi tao GPIO pin lam input
 */
void gpio_init_input(GPIO_TypeDef* GPIOx, uint16_t pin, uint8_t mode)
{
    // BUOC 1: BAT CLOCK cho GPIO Port
    gpio_enable_clock(GPIOx);
    
    // BUOC 2: CAU HINH PIN LAM INPUT theo mode da chon
    uint32_t config_value;
    
    switch(mode)
    {
        case GPIO_INPUT_FLOATING:
            config_value = 0x4;                     // Input floating (CNF=01, MODE=00)
            break;
            
        case GPIO_INPUT_PULLUP:
            config_value = 0x8;                     // Input pull-up/down (CNF=10, MODE=00)
            break;
            
        case GPIO_INPUT_PULLDOWN:
            config_value = 0x8;                     // Input pull-up/down (CNF=10, MODE=00)
            break;
            
        default:
            config_value = 0x4;                     // Mac dinh la floating
            break;
    }
    
    // CAU HINH THANH GHI CRL/CRH
    if(pin < 8)                                     // Pin 0-7 su dung thanh ghi CRL 
    {
        GPIOx->CRL &= ~(0xF << (pin * 4));          // Xoa 4 bit cau hinh cu
        GPIOx->CRL |= (config_value << (pin * 4));  // Dat gia tri cau hinh moi
    }
    else if(pin < 16)                               // Pin 8-15 su dung thanh ghi CRH 
    {
        uint8_t pin_offset = pin - 8;               // Tinh vi tri pin trong CRH (0-7)
        GPIOx->CRH &= ~(0xF << (pin_offset * 4));   // Xoa 4 bit cau hinh cu
        GPIOx->CRH |= (config_value << (pin_offset * 4)); // Dat gia tri cau hinh moi
    }
    
    // BUOC 3: CAU HINH PULL-UP/PULL-DOWN neu can
    if(mode == GPIO_INPUT_PULLUP)
    {
        GPIOx->ODR |= (1 << pin);                   // Set ODR = 1 cho pull-up
    }
    else if(mode == GPIO_INPUT_PULLDOWN)
    {
        GPIOx->ODR &= ~(1 << pin);                  // Set ODR = 0 cho pull-down
    }
}

/**
 * @brief Ghi gia tri len GPIO pin
 */
void gpio_write_pin(GPIO_TypeDef* GPIOx, uint16_t pin, uint8_t state)
{
    if(state == HIGH)                               // Set pin HIGH
    {
        GPIOx->ODR |= (1 << pin);                   // Set bit
    }
    else                                            // Set pin LOW
    {
        GPIOx->ODR &= ~(1 << pin);                  // Clear bit
    }
}

/**
 * @brief Doc gia tri tu GPIO pin
 */
uint8_t gpio_read_pin(GPIO_TypeDef* GPIOx, uint16_t pin)
{
    if(GPIOx->IDR & (1 << pin))                     // Kiem tra bit trong IDR
    {
        return HIGH;                                // Pin dang o muc HIGH
    }
    else
    {
        return LOW;                                 // Pin dang o muc LOW
    }
}

/**
 * @brief Dao trang thai GPIO pin (toggle)
 */
void gpio_toggle_pin(GPIO_TypeDef* GPIOx, uint16_t pin)
{
    if(GPIOx->ODR & (1 << pin))                     // Neu pin dang HIGH
    {
        GPIOx->ODR &= ~(1 << pin);                  // Set pin ve LOW
    }
    else                                            // Neu pin dang LOW
    {
        GPIOx->ODR |= (1 << pin);                   // Set pin ve HIGH
    }
}