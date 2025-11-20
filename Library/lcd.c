#include "lcd.h"
#include "gpio.h"
#include "systick.h"

/**
 * @brief Gửi 4 bit dữ liệu tới LCD
 * @param data: 4 bit dữ liệu (chỉ sử dụng bit 0-3)
 * 
 * Quy trình gửi dữ liệu 4-bit:
 *    ┌─────┐      ┌─────┐
 *    │  E  │ HIGH │     │ LOW
 *    │     │────→ │     │────→ LCD đọc dữ liệu
 *    └─────┘      └─────┘
 *       ↑           ↑
 *   Đặt data   Cạnh xuống
 */
static void LCD_Write4Bits(uint8_t data) {
    // Xóa 4 bit PA4-PA7 và ghi 4 bit mới cùng lúc
    LCD_DATA_PORT->ODR = (LCD_DATA_PORT->ODR & ~(0xF << LCD_START_DATA_PIN)) | ((data & 0x0F) << LCD_START_DATA_PIN);
    Delay_us(100);
    // Tạo xung Enable
    gpio_write_pin(LCD_E_PORT, LCD_E_PIN, HIGH);
    Delay_us(100);
    gpio_write_pin(LCD_E_PORT, LCD_E_PIN, LOW);
    Delay_us(100);
}

/**
 * @brief Gửi 1 byte (8 bit) tới LCD
 * @param data: Dữ liệu 8 bit cần gửi
 * @param mode: 0 = Lệnh (Command), 1 = Dữ liệu (Data)
 * 
 * Gửi 8-bit qua 4-bit interface:
 *    8-bit data: [D7 D6 D5 D4 | D3 D2 D1 D0]
 *                     ↓              ↓
 *    Gửi lần 1:  [D7 D6 D5 D4]  (4 bit cao)
 *    Gửi lần 2:  [D3 D2 D1 D0]  (4 bit thấp)
 */
static void LCD_Send(uint8_t data, uint8_t mode) {
    // MODE = 0: Command , 1: Data
    gpio_write_pin(LCD_RS_PORT, LCD_RS_PIN, mode ? HIGH : LOW);
    LCD_Write4Bits(data >> 4);      // Gửi 4 bit cao
    LCD_Write4Bits(data & 0x0F);    // Gửi 4 bit thấp
}

/* =============================================================================
 * HÀM KHỞI TẠO LCD
 * ============================================================================= */

/**
 * @brief Khởi tạo GPIO cho LCD
 */
static void LCD_GPIO_Init(void) {
    gpio_init_output(LCD_RS_PORT, LCD_RS_PIN);
    gpio_init_output(LCD_E_PORT, LCD_E_PIN);
    gpio_init_output(LCD_D4_PORT, LCD_D4_PIN);
    gpio_init_output(LCD_D5_PORT, LCD_D5_PIN);
    gpio_init_output(LCD_D6_PORT, LCD_D6_PIN);
    gpio_init_output(LCD_D7_PORT, LCD_D7_PIN);
    
    gpio_write_pin(LCD_RS_PORT, LCD_RS_PIN, LOW);
    gpio_write_pin(LCD_E_PORT, LCD_E_PIN, LOW);
}

/**
 * @brief Khởi tạo LCD theo đúng trình tự chuẩn
 * 
 * Trình tự khởi tạo HD44780:
 *    Power On → Wait 15ms → 8-bit × 3 → 4-bit → Commands
 *       │         │          │         │         │
 *       └─────────┼──────────┼─────────┼─────────┼──→ LCD Ready
 *                 ▼          ▼         ▼         ▼
 *              Stable    Initialize  Switch   Configure
 *              Power      8-bit     to 4-bit   Display
 */
void LCD_Init(void) {
    // Khởi tạo GPIO
    LCD_GPIO_Init();
    Delay_ms(20);

    gpio_write_pin(LCD_RS_PORT, LCD_RS_PIN, LOW);
    
    /* ─────────────────────────────────────────────────────────────
     * BƯỚC 1-3: Khởi tạo 8-bit mode (3 lần)
     * ─────────────────────────────────────────────────────────────
     *  0x03 = 0011₂ = Function Set 8-bit
     *  ┌─ 0 0 1 1 ────┐
     *  │ D7 D6 D5 D4  │  Gửi lệnh Function Set
     *  └──────────────┘
     */
    LCD_Write4Bits(0x03);  // 0x03 = 0011₂ → Function Set 8-bit (lần 1)
    Delay_ms(5);
    
    LCD_Write4Bits(0x03);  // 0x03 = 0011₂ → Function Set 8-bit (lần 2)
    Delay_us(150);
    
    LCD_Write4Bits(0x03);  // 0x03 = 0011₂ → Function Set 8-bit (lần 3)
    Delay_us(100);

    /* ─────────────────────────────────────────────────────────────
     * BƯỚC 4: Chuyển sang 4-bit mode
     * ─────────────────────────────────────────────────────────────
     *  0x02 = 0010₂ = Function Set 4-bit
     *  ┌─ 0 0 1 0 ────┐
     *  │ D7 D6 D5 D4  │  Chuyển sang 4-bit interface
     *  └──────────────┘
     */
    LCD_Write4Bits(0x02);  // 0x02 = 0010₂ → Function Set 4-bit
    Delay_us(500);

    /* ═════════════════════════════════════════════════════════════
     * TỪ GIÂY TRỞ ĐI, SỬ DỤNG 4-BIT INTERFACE
     * ═════════════════════════════════════════════════════════════ */

    LCD_Send(0x28, 0);  // Function Set: 4-bit, 2 dòng, font 5×8
    Delay_us(100);
    LCD_Send(0x08, 0);  // Display OFF
    Delay_us(100);
    LCD_Send(0x01, 0);  // Clear Display
    Delay_ms(3);
    LCD_Send(0x06, 0);  // Entry Mode: cursor tăng, không dịch
    Delay_us(100);
    LCD_Send(0x0C, 0);  // Display ON, Cursor OFF, Blink 
    Delay_us(100);
}

/* =============================================================================
 * CÁC HÀM ĐIỀU KHIỂN LCD
 * ============================================================================= */

/**
 * @brief Xóa toàn bộ màn hình LCD
 * 
 * Clear Display Command (0x01):
 *  Before: ┌─────────────────┐    After: ┌─────────────────┐
 *          │ Hello World     │     →     │                 │
 *          │ STM32F103   ■   │           │ ■               │
 *          └─────────────────┘           └─────────────────┘
 *             Data visible                 All cleared + cursor home
 */
void LCD_Clear(void) {
    LCD_Send(0x01, 0);  // 0x01 = Clear Display command
    Delay_ms(3);        // Clear command cần thời gian dài nhất (~1.64ms)
}

/**
 * @brief Dat vi tri con tro tren LCD 20x4
 * @param row: Dong (0-3 cho 4 dong)
 * @param col: Cot (0-19 cho 20 cot)
 * 
 * DDRAM Address Map cho LCD 20x4:
 * ┌─────────────────────────────────────────────────────
 * │ Dong 1: 0x00-0x13 (0x80 + 0-19) (128 + 0-19 = 128-147)
 * │ Dong 2: 0x40-0x53 (0xC0 + 0-19) (192 + 0-19 = 192-211)
 * │ Dong 3: 0x14-0x27 (0x94 + 0-19) (148 + 0-19 = 148-167)
 * │ Dong 4: 0x54-0x67 (0xD4 + 0-19) (212 + 0-19 = 212-231)
 * └─────────────────────────────────────────────────────
 */
void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t address;
    
    // Gioi han kiem tra
    if(row > 3) row = 3;    // Toi da 4 dong (0-3)
    if(col > 19) col = 19;  // Toi da 20 cot (0-19)
    
    switch(row) {
        case 0:
            address = 0x80 + col;  // Dong 1: 0x80 + col (0x80-0x93)
            break;
        case 1:
            address = 0xC0 + col;  // Dong 2: 0xC0 + col (0xC0-0xD3)
            break;
        case 2:
            address = 0x94 + col;  // Dong 3: 0x94 + col (0x94-0xA7)
            break;
        case 3:
            address = 0xD4 + col;  // Dong 4: 0xD4 + col (0xD4-0xE7)
            break;
        default:
            address = 0x80;        // Mac dinh dong 1, cot 0
            break;
    }
    
    LCD_Send(address, 0);  // RS=0 (Command mode) de gui Set DDRAM Address
}

/**
 * @brief In một ký tự lên LCD
 * @param c: Ký tự cần in
 * 
 * Write Data to DDRAM:
 *  ASCII 'A' = 0x41 = 0100 0001₂
 *  Effect: Cursor: [pos] → Write 'A' → [pos+1]
 *          Display: "____" → "A___"
 */
void LCD_PrintChar(char c) {
    LCD_Send(c, 1);  // Gửi ký tự với RS=1 (Data mode)
}

/**
 * @brief In chuỗi ký tự lên LCD
 * @param str: Con trỏ tới chuỗi cần in
 * 
 * String printing sequence:
 *  Input: "Hello"
 *  Process: 'H' → 'e' → 'l' → 'l' → 'o' → '\0' (stop)
 *           ↓     ↓     ↓     ↓     ↓
 *  Display: H     He    Hel   Hell  Hello
 *  Cursor:  [1]   [2]   [3]   [4]   [5]
 */
void LCD_PrintString(char *str) {
    while (*str) {           // Lặp đến '\0'
        LCD_PrintChar(*str); // In từng ký tự
        str++;               // Tiến tới ký tự tiếp theo
    }
}
/**
 * @brief Bật/tắt hiển thị LCD
 * @param state: 1 = bật, 0 = tắt
 */
void LCD_Display(uint8_t state) {
    if (state) {
        LCD_Send(0x0C, 0);  // Display ON, Cursor OFF, Blink OFF
    } else {
        LCD_Send(0x08, 0);  // Display OFF
    }
}

/**
 * @brief Bật/tắt con trỏ
 * @param state: 1 = bật, 0 = tắt
 */
void LCD_Cursor(uint8_t state) {
    if (state) {
        LCD_Send(0x0E, 0);  // Display ON, Cursor ON, Blink OFF
    } else {
        LCD_Send(0x0C, 0);  // Display ON, Cursor OFF, Blink OFF
    }
}

/**
 * @brief Bật/tắt nhấp nháy con trỏ
 * @param state: 1 = bật, 0 = tắt
 */
void LCD_Blink(uint8_t state) {
    if (state) {
        LCD_Send(0x0F, 0);  // Display ON, Cursor ON, Blink ON
    } else {
        LCD_Send(0x0C, 0);  // Display ON, Cursor OFF, Blink OFF
    }
}