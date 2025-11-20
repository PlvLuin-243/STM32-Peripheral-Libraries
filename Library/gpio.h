#ifndef GPIO_H
#define GPIO_H

#include "stm32f10x.h"

/*
 * ===========================================================================
 * THU VIEN GPIO CHO STM32F1
 * ===========================================================================
 */

/*
 * ---------------------------------------------------------------------------
 * GPIO STATE DEFINITIONS - DINH NGHIA TRANG THAI GPIO
 * ---------------------------------------------------------------------------
 */
#define HIGH   1
#define LOW    0

/*
 * ---------------------------------------------------------------------------
 * GPIO PIN DEFINITIONS - DINH NGHIA CAC CHAN GPIO
 * ---------------------------------------------------------------------------
 */
#define GPIO_PIN_0      0      /* Pin 0 */
#define GPIO_PIN_1      1      /* Pin 1 */
#define GPIO_PIN_2      2      /* Pin 2 */
#define GPIO_PIN_3      3      /* Pin 3 */
#define GPIO_PIN_4      4      /* Pin 4 */
#define GPIO_PIN_5      5      /* Pin 5 */
#define GPIO_PIN_6      6      /* Pin 6 */
#define GPIO_PIN_7      7      /* Pin 7 */
#define GPIO_PIN_8      8      /* Pin 8 */
#define GPIO_PIN_9      9      /* Pin 9 */
#define GPIO_PIN_10     10     /* Pin 10 */
#define GPIO_PIN_11     11     /* Pin 11 */
#define GPIO_PIN_12     12     /* Pin 12 */
#define GPIO_PIN_13     13     /* Pin 13 */
#define GPIO_PIN_14     14     /* Pin 14 */
#define GPIO_PIN_15     15     /* Pin 15 */

/*
 * ---------------------------------------------------------------------------
 * GPIO INPUT MODE DEFINITIONS - DINH NGHIA CHE DO INPUT
 * ---------------------------------------------------------------------------
 */
#define GPIO_INPUT_FLOATING     0      /* Input floating - khong co dien tro keo */
#define GPIO_INPUT_PULLUP       1      /* Input pull-up - co dien tro keo len */
#define GPIO_INPUT_PULLDOWN     2      /* Input pull-down - co dien tro keo xuong */

/*
 * ---------------------------------------------------------------------------
 * FUNCTION DECLARATIONS - KHAI BAO HAM
 * ---------------------------------------------------------------------------
 */
/**
 * @brief Bat clock cho GPIO port
 * @param GPIOx: Con tro den GPIO port (GPIOA, GPIOB, GPIOC, ...)
 *
 * @note Ham nay tu dong duoc goi trong cac ham khoi tao GPIO
 */
void gpio_enable_clock(GPIO_TypeDef* GPIOx);

/**
 * @brief Khoi tao GPIO pin lam output
 * @param GPIOx: Con tro den GPIO port (GPIOA, GPIOB, GPIOC, ...)
 * @param pin: Chan GPIO can cau hinh (GPIO_PIN_0 den GPIO_PIN_15)
 * 
 * @note Ham nay tu dong:
 *       - Bat clock cho GPIO port
 *       - Cau hinh pin lam output push-pull
 *       - Toc do mac dinh: 2MHz
 * 
 * @example:
 * gpio_init_output(GPIOB, GPIO_PIN_12);  // LED tren PB12
 * gpio_init_output(GPIOC, GPIO_PIN_13);  // LED built-in tren PC13
 */
void gpio_init_output(GPIO_TypeDef* GPIOx, uint16_t pin);

/**
 * @brief Khoi tao GPIO pin lam input
 * @param GPIOx: Con tro den GPIO port (GPIOA, GPIOB, GPIOC, ...)
 * @param pin: Chan GPIO can cau hinh (GPIO_PIN_0 den GPIO_PIN_15)
 * @param mode: Che do input (GPIO_INPUT_FLOATING, GPIO_INPUT_PULLUP, GPIO_INPUT_PULLDOWN)
 * 
 * @note Ham nay tu dong:
 *       - Bat clock cho GPIO port
 *       - Cau hinh pin lam input theo che do da chon
 * 
 * @example:
 * gpio_init_input(GPIOA, GPIO_PIN_0, GPIO_INPUT_FLOATING);   // Sensor tren PA0 (floating)
 * gpio_init_input(GPIOB, GPIO_PIN_1, GPIO_INPUT_PULLUP);     // Button tren PB1 (pull-up)
 * gpio_init_input(GPIOC, GPIO_PIN_2, GPIO_INPUT_PULLDOWN);   // Switch tren PC2 (pull-down)
 */
void gpio_init_input(GPIO_TypeDef* GPIOx, uint16_t pin, uint8_t mode);

/**
 * @brief Ghi gia tri len GPIO pin
 * @param GPIOx: Con tro den GPIO port
 * @param pin: Chan GPIO can ghi
 * @param state: Gia tri can ghi (HIGH hoac LOW)
 * 
 * @example:
 * gpio_write_pin(GPIOB, GPIO_PIN_12, HIGH);  // Bat LED
 * gpio_write_pin(GPIOB, GPIO_PIN_12, LOW);   // Tat LED
 */
void gpio_write_pin(GPIO_TypeDef* GPIOx, uint16_t pin, uint8_t state);

/**
 * @brief Doc gia tri tu GPIO pin
 * @param GPIOx: Con tro den GPIO port
 * @param pin: Chan GPIO can doc
 * @return: Gia tri doc duoc (HIGH hoac LOW)
 * 
 * @example:
 * uint8_t button_state = gpio_read_pin(GPIOA, GPIO_PIN_0);
 * if(button_state == HIGH) {
 *     // Button duoc nhan
 * }
 */
uint8_t gpio_read_pin(GPIO_TypeDef* GPIOx, uint16_t pin);

/**
 * @brief Dao trang thai GPIO pin (toggle)
 * @param GPIOx: Con tro den GPIO port
 * @param pin: Chan GPIO can dao trang thai
 * 
 * @note Neu pin dang o muc HIGH thi chuyen ve LOW va nguoc lai
 * 
 * @example:
 * gpio_toggle_pin(GPIOB, GPIO_PIN_12);  // Dao trang thai LED
 */
void gpio_toggle_pin(GPIO_TypeDef* GPIOx, uint16_t pin);

#endif /* GPIO_H */