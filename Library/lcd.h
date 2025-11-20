#ifndef LCD_H
#define LCD_H

#include "stm32f10x.h"

/* =============================================================================
 * CẤU HÌNH KẾT NỐI LCD 16x2 VỚI STM32F103
 * =============================================================================
 * LCD được kết nối theo chế độ 4-bit để tiết kiệm chân GPIO:
 * - RS (Register Select): PA2  - Chọn ghi lệnh (0) hay dữ liệu (1)
 * - E (Enable):          PA3  - Xung clock để LCD đọc dữ liệu
 * - D4:                  PA4  - Bit dữ liệu 4 (LSB của nibble)
 * - D5:                  PA5  - Bit dữ liệu 5
 * - D6:                  PA6  - Bit dữ liệu 6  
 * - D7:                  PA7  - Bit dữ liệu 7 (MSB của nibble)
 * - VSS, RW:             GND  - Mass và chế độ ghi
 * - VDD:                 5V   - Nguồn cung cấp
 * - V0:                  Biến trở 10K - Điều chỉnh độ tương phản
 * ============================================================================= */

// Định nghĩa chân kết nối LCD
#define LCD_RS_PORT     GPIOA
#define LCD_RS_PIN      2

#define LCD_E_PORT      GPIOA  
#define LCD_E_PIN       3

// Định nghĩa PORT cho 4 bit data (D4-D7)
#define LCD_DATA_PORT   GPIOA    // Có thể thay đổi thành GPIOB, GPIOC...
#define LCD_D4_PORT     LCD_DATA_PORT
#define LCD_D4_PIN      4

#define LCD_D5_PORT     LCD_DATA_PORT
#define LCD_D5_PIN      5

#define LCD_D6_PORT     LCD_DATA_PORT
#define LCD_D6_PIN      6

#define LCD_D7_PORT     LCD_DATA_PORT
#define LCD_D7_PIN      7

#define LCD_START_DATA_PIN   4

// Khai báo các hàm LCD
void LCD_Init(void);
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_PrintChar(char c);
void LCD_PrintString(char *str);
void LCD_Display(uint8_t state);
void LCD_Cursor(uint8_t state);
void LCD_Blink(uint8_t state);

#endif // LCD_H