#include "stm32f10x.h"
#include "gpio.h"
#include "can.h"
#include "systick.h"
#include "timer.h"
#include "lcd.h"
#include "adc.h"
#include "uart.h"
#include <stdio.h>
#include <stdbool.h>

// =================== ENUMS ===================
typedef enum
{
    MODE_SETPOINT_UART = 0,
    MODE_SETPOINT_ADC,
} master_mode_t;

typedef enum
{
    CONTROL_TARGET_SLAVE1 = 0,
    CONTROL_TARGET_SLAVE2,
    CONTROL_TARGET_ALL,
} control_target_t;
typedef enum
{
    STATUS_IDLE = 0x00,
    STATUS_RUNNING = 0x01,
    STATUS_STOPPED = 0x02,
    STATUS_ERROR = 0xFF
} slave_status_t;

// =================== DEFINES ===================
#define BUTTON_MODE_PORT GPIOB
#define BUTTON_MODE_PIN GPIO_PIN_10
#define BUTTON_SLAVE1_PORT GPIOB
#define BUTTON_SLAVE1_PIN GPIO_PIN_0
#define BUTTON_SLAVE2_PORT GPIOB
#define BUTTON_SLAVE2_PIN GPIO_PIN_1
#define BUTTON_CONTROL_PORT GPIOB
#define BUTTON_CONTROL_PIN GPIO_PIN_11

#define DEBOUNCE_DELAY_MS 25
#define LCD_UPDATE_PERIOD_MS 100
#define CONNECTION_TIMEOUT_MS 5000

// UART DMA configuration
#define UART_RX_BUFFER_SIZE 64

// ADC and conversion constants
#define ADC_MAX_VALUE 4090
#define PERCENTAGE_MAX 100

// SLAVE1 (Motor) constants
#define MOTOR_MAX_RPM 6000

// SLAVE2 (Buck) constants
#define BUCK_MAX_VOLT 900 // 10.00V in 0.01V units

// Status Register (SR) bit definitions
#define SR_BTN_SLAVE1 (1 << 0)      // Bit 0: Button Slave1 pressed
#define SR_BTN_SLAVE2 (1 << 1)      // Bit 1: Button Slave2 pressed
#define SR_BTN_MODE (1 << 2)        // Bit 2: Button Mode pressed
#define SR_BTN_CONTROL (1 << 3)     // Bit 3: Button Control pressed
#define SR_LCD_UPDATE (1 << 4)      // Bit 4: LCD update flag
#define SR_ADC_UPDATE (1 << 5)      // Bit 5: ADC update flag
#define SR_TIMER_100MS (1 << 6)     // Bit 6: Timer 100ms flag
#define SR_CAN_RX (1 << 7)          // Bit 7: CAN message received
#define SR_STATUS_CHECK (1 << 8)    // Bit 8: Status check flag (every 3s)
#define SR_SETPOINT_UPDATE (1 << 9) // Bit 9: Setpoint update flag (every 100ms)
#define SR_UART_UPDATE (1 << 10)    // Bit 10: UART data received

// =================== STRUCTS ===================
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} button_t;

typedef struct
{
    uint8_t status;
    uint16_t current_setpoint;
} slave_info_t;

typedef struct
{
    master_mode_t current_mode;
    control_target_t control_target;
    uint8_t uart_setpoint;
    uint16_t adc_value;
    uint8_t adc_setpoint;
} master_control_t;

// =================== GLOBAL VARIABLES ===================
// Status Register - Contains all system flags
volatile uint16_t SR = 0x0000;

// Timer variables
volatile uint16_t lcd_update_counter = 0;
volatile uint16_t status_check_counter = 0;

// Control structures
master_control_t master_ctrl;
button_t button_mode;
button_t button_slave1;
button_t button_slave2;
button_t button_control;
slave_info_t slave1_info;
slave_info_t slave2_info;

// UART DMA variables
volatile uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
volatile uint8_t uart_tx_buffer[128];      // Buffer for TX
volatile uint8_t uart_received = 0;        // Flag: UART data received
volatile uint16_t uart_rx_length = 0;      // Number of bytes received
extern volatile uint8_t uart1_rx_complete;
extern volatile uint8_t uart1_tx_complete;

// =================== FUNCTION DECLARATIONS ===================
// System initialization
void system_init(void);
void buttons_init(void);
void variables_init(void);
void adc_gpio_init(void);
void adc_process(void);
void uart_process(void);

// Button handling
void button_init(button_t *btn, GPIO_TypeDef *port, uint16_t pin);
uint8_t button_debounce(button_t *btn);
void button_process_all(void);
void button_mode_handler(void);
void button_slave1_handler(void);
void button_slave2_handler(void);
void button_control_handler(void);

// CAN communication
void can_send_command(can_command_t cmd);
void can_send_setpoint(uint8_t slave_id, uint16_t adc_val);
void can_send_setpoint_all(uint16_t adc_val);
void can_send_status_all(void);
void send_setpoint_to_target(uint8_t value);
void send_setpoint_to_slaves(void);
void send_setpoint_to_running_slaves(void);
void can_rx_callback(can_message_t *msg, uint8_t fmi);

// Slave management
void slave_info_init(slave_info_t *slave, uint8_t id);
void slave_update_connection(slave_info_t *slave);

// LCD display
void lcd_display_startup(void);
void lcd_update_display(void);
void lcd_display_mode_setpoint_uart(void);
void lcd_display_mode_setpoint_adc(void);

// Timer callback
void Timer4_IRQ_Callback(void);

// Interrupt handlers
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI15_10_IRQHandler(void);

// =================== MAIN FUNCTION ===================
int main(void)
{
    system_init();
    Delay_ms(100); // Wait for slaves to initialize
    can_send_status_all();
    lcd_display_startup();

    while (1)
    {
        // Process button flags
        if (SR & SR_BTN_MODE)
        {
            SR &= ~SR_BTN_MODE; // Clear flag
            if (button_debounce(&button_mode))
            {
                button_mode_handler();
            }
        }

        if (SR & SR_BTN_SLAVE1)
        {
            SR &= ~SR_BTN_SLAVE1; // Clear flag
            if (button_debounce(&button_slave1))
            {
                button_slave1_handler();
            }
        }

        if (SR & SR_BTN_SLAVE2)
        {
            SR &= ~SR_BTN_SLAVE2; // Clear flag
            if (button_debounce(&button_slave2))
            {
                button_slave2_handler();
            }
        }

        if (SR & SR_BTN_CONTROL)
        {
            SR &= ~SR_BTN_CONTROL; // Clear flag
            if (button_debounce(&button_control))
            {
                button_control_handler();
            }
        }

        // Process ADC update
        if (SR & SR_ADC_UPDATE)
        {
            SR &= ~SR_ADC_UPDATE; // Clear flag
            adc_process();        // Always process ADC to update values
        }

        // Process UART update (IDLE line detection)
        if (uart_received)
        {
            uart_process(); // Process UART received data
        }

        // Process setpoint update (every 100ms)
        if (SR & SR_SETPOINT_UPDATE)
        {
            SR &= ~SR_SETPOINT_UPDATE; // Clear flag
            send_setpoint_to_running_slaves();
        }

        // Process status check
        if (SR & SR_STATUS_CHECK)
        {
            SR &= ~SR_STATUS_CHECK; // Clear flag
            can_send_status_all();
        }

        // Process CAN RX
        if (SR & SR_CAN_RX)
        {
            SR &= ~SR_CAN_RX; // Clear flag
            // Data already updated in can_rx_callback()
            // Can add additional processing here if needed
        }

        // Process LCD update
        if (SR & SR_LCD_UPDATE)
        {
            SR &= ~SR_LCD_UPDATE; // Clear flag
            lcd_update_display();
        }
    }
}

// =================== SYSTEM INITIALIZATION ===================
void system_init(void)
{
    SysTick_Init();
    LCD_Init();
    Timer_IRQ_Init(TIM4, 71, 999); // 1ms interrupt

    can_init();
    can_master_setup_filters(FB_SLAVE1, FB_SLAVE2);
    buttons_init();
    adc_gpio_init();
    DMA1_Channel1_Config();
    ADC1_DMA_Config(1);

    // Initialize UART DMA with IDLE line detection
    UART_Init(1, 9600);
    UART_DMA_TX_Init(1);
    UART_DMA_RX_Init(1, (uint8_t *)uart_rx_buffer, UART_RX_BUFFER_SIZE);
    UART_DMA_Enable(1);
    
    // Enable IDLE line interrupt for USART1
    USART1->CR1 |= (1 << 4);  // IDLEIE: IDLE interrupt enable
    NVIC_EnableIRQ(USART1_IRQn);
    
    variables_init();
}

void adc_gpio_init(void)
{
    // Enable GPIOA clock
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA1: ADC Channel 1 (Analog Input)
    GPIOA->CRL &= ~(0xF << (1 * 4)); // Analog mode (0000)
}

void adc_process(void)
{
    master_ctrl.adc_value = adc_value;
    master_ctrl.adc_setpoint = (uint8_t)((adc_value * PERCENTAGE_MAX) / ADC_MAX_VALUE);

    if (master_ctrl.adc_setpoint > PERCENTAGE_MAX)
        master_ctrl.adc_setpoint = PERCENTAGE_MAX;
}

void uart_process(void)
{
    // Protocol: "Duty:XXX" (e.g., "Duty:50", "Duty:100")
    // Parse the received string to extract duty value
    
    // Only process in UART mode
        // Null-terminate the received string
        uart_rx_buffer[uart_rx_length] = '\0';
        
    // Check if string starts with "Duty:"
    if (uart_rx_length >= 6 &&
        uart_rx_buffer[0] == 'D' &&
        uart_rx_buffer[1] == 'u' &&
        uart_rx_buffer[2] == 't' &&
        uart_rx_buffer[3] == 'y' &&
        uart_rx_buffer[4] == ':')
    {
        // Extract numeric value after "Duty:"
        uint8_t value = 0;
        uint8_t i = 5; // Start after "Duty:"
        
        // Parse digits
        while (i < uart_rx_length && uart_rx_buffer[i] >= '0' && uart_rx_buffer[i] <= '9')
        {
            value = value * 10 + (uart_rx_buffer[i] - '0');
            i++;
        }
        
        // Limit to 0-100%
        if (value > 100)
            value = 100;
        
        master_ctrl.uart_setpoint = value;
        
        // Send response via UART DMA TX
        if (uart1_tx_complete)
        {
            char slave1_str[40] = {0};
            char slave2_str[40] = {0};
            
            // Format Slave1 status
            if (slave1_info.status == STATUS_RUNNING)
            {
                // Show RPM setpoint for running slave
                snprintf(slave1_str, sizeof(slave1_str), "RUN RPM:%d", slave1_info.current_setpoint);
            }
            else if (slave1_info.status == STATUS_STOPPED)
            {
                snprintf(slave1_str, sizeof(slave1_str), "STOP");
            }
            else if (slave1_info.status == STATUS_ERROR)
            {
                snprintf(slave1_str, sizeof(slave1_str), "ERR");
            }
            else
            {
                snprintf(slave1_str, sizeof(slave1_str), "IDLE");
            }
            
            // Format Slave2 status
            if (slave2_info.status == STATUS_RUNNING)
            {
                // Show Voltage setpoint for running slave (in 0.01V units)
                uint16_t volt_int = slave2_info.current_setpoint / 100;
                uint16_t volt_dec = (slave2_info.current_setpoint / 10) % 10;
                snprintf(slave2_str, sizeof(slave2_str), "RUN VOLT:%d.%dV", volt_int, volt_dec);
            }
            else if (slave2_info.status == STATUS_STOPPED)
            {
                snprintf(slave2_str, sizeof(slave2_str), "STOP");
            }
            else if (slave2_info.status == STATUS_ERROR)
            {
                snprintf(slave2_str, sizeof(slave2_str), "ERR");
            }
            else
            {
                snprintf(slave2_str, sizeof(slave2_str), "IDLE");
            }
            
            // Format response: "Received: Duty:XX% | Slave1: RUN RPM:1234 | Slave2: RUN VOLT:5.5V\r\n"
            int len = snprintf((char*)uart_tx_buffer, sizeof(uart_tx_buffer),
                              "Received: Duty:%d%% | S1: %s | S2: %s\r\n",
                              value, slave1_str, slave2_str);
            
            // Send via DMA using library function
            UART_DMA_SendData(1, (uint8_t*)uart_tx_buffer, len);
        }
    } 
    // Clear received flag for next reception
    uart_received = 0;
}

void buttons_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;

    button_init(&button_mode, BUTTON_MODE_PORT, BUTTON_MODE_PIN);
    button_init(&button_slave1, BUTTON_SLAVE1_PORT, BUTTON_SLAVE1_PIN);
    button_init(&button_slave2, BUTTON_SLAVE2_PORT, BUTTON_SLAVE2_PIN);
    button_init(&button_control, BUTTON_CONTROL_PORT, BUTTON_CONTROL_PIN);

    // EXTI0 for Slave1 button (PB0)
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    AFIO->EXTICR[0] |= AFIO_EXTICR1_EXTI0_PB;
    EXTI->IMR |= EXTI_IMR_MR0;
    EXTI->FTSR |= EXTI_FTSR_TR0;
    NVIC_EnableIRQ(EXTI0_IRQn);

    // EXTI1 for Slave2 button (PB1)
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI1;
    AFIO->EXTICR[0] |= AFIO_EXTICR1_EXTI1_PB;
    EXTI->IMR |= EXTI_IMR_MR1;
    EXTI->FTSR |= EXTI_FTSR_TR1;
    NVIC_EnableIRQ(EXTI1_IRQn);

    // EXTI10 for Mode button (PB10)
    AFIO->EXTICR[2] &= ~AFIO_EXTICR3_EXTI10;
    AFIO->EXTICR[2] |= AFIO_EXTICR3_EXTI10_PB;
    EXTI->IMR |= EXTI_IMR_MR10;
    EXTI->FTSR |= EXTI_FTSR_TR10;

    // EXTI11 for Control button (PB11)
    AFIO->EXTICR[2] &= ~AFIO_EXTICR3_EXTI11;
    AFIO->EXTICR[2] |= AFIO_EXTICR3_EXTI11_PB;
    EXTI->IMR |= EXTI_IMR_MR11;
    EXTI->FTSR |= EXTI_FTSR_TR11;

    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void variables_init(void)
{
    master_ctrl.current_mode = MODE_SETPOINT_UART;
    master_ctrl.control_target = CONTROL_TARGET_ALL;
    master_ctrl.uart_setpoint = 0;
    master_ctrl.adc_value = 0;
    master_ctrl.adc_setpoint = 0;

    slave_info_init(&slave1_info, 1);
    slave_info_init(&slave2_info, 2);
}

// =================== BUTTON HANDLING ===================
void button_init(button_t *btn, GPIO_TypeDef *port, uint16_t pin)
{
    btn->port = port;
    btn->pin = pin;
    gpio_init_input(port, pin, GPIO_INPUT_PULLUP);
}

uint8_t button_debounce(button_t *btn)
{
    if (gpio_read_pin(btn->port, btn->pin) == LOW)
    {
        Delay_ms(DEBOUNCE_DELAY_MS);
        if (gpio_read_pin(btn->port, btn->pin) == LOW)
        {
            return 1;
        }
    }
    return 0;
}

void button_process_all(void)
{
    // TODO: Implementation
}

void button_mode_handler(void)
{
    // Chuyển đổi mode giữa UART và ADC
    if (master_ctrl.current_mode == MODE_SETPOINT_UART)
    {
        master_ctrl.current_mode = MODE_SETPOINT_ADC;
    }
    else
    {
        master_ctrl.current_mode = MODE_SETPOINT_UART;
    }

    // Gửi setpoint ngay khi chuyển mode
    send_setpoint_to_slaves();
}

void button_slave1_handler(void)
{
    // Toggle START/STOP for SLAVE1
    if (slave1_info.status == STATUS_RUNNING)
    {
        // If running, send STOP command
        can_send_command(CMD_STOP_SLAVE1);
    }
    else
    {
        // If stopped or idle, send START command
        can_send_command(CMD_START_SLAVE1);
        Delay_ms(10); // Small delay for START command processing
        // Send current ADC value after START
        uint16_t adc_val = (master_ctrl.current_mode == MODE_SETPOINT_ADC) ? master_ctrl.adc_value : ((master_ctrl.uart_setpoint * ADC_MAX_VALUE) / PERCENTAGE_MAX);
        can_send_setpoint(1, adc_val);
    }
}

void button_slave2_handler(void)
{
    // Toggle START/STOP for SLAVE2
    if (slave2_info.status == STATUS_RUNNING)
    {
        // If running, send STOP command
        can_send_command(CMD_STOP_SLAVE2);
    }
    else
    {
        // If stopped or idle, send START command
        can_send_command(CMD_START_SLAVE2);
        Delay_ms(10); // Small delay for START command processing
        // Send current ADC value after START
        uint16_t adc_val = (master_ctrl.current_mode == MODE_SETPOINT_ADC) ? master_ctrl.adc_value : ((master_ctrl.uart_setpoint * ADC_MAX_VALUE) / PERCENTAGE_MAX);
        can_send_setpoint(2, adc_val);
    }
}

void button_control_handler(void)
{
    // Chuyển đổi control target: All(2) -> Slave2(1) -> Slave1(0) -> All(2)...
    master_ctrl.control_target = (master_ctrl.control_target + 2) % 3;
}

// =================== CAN COMMUNICATION ===================
void can_send_command(can_command_t cmd)
{
    can_message_t msg;
    msg.id = cmd;
    msg.rtr = false;
    msg.length = 0; // Command only, no data

    can_transmit(&msg);
}

void can_send_setpoint(uint8_t slave_id, uint16_t adc_val)
{
    can_message_t msg;
    msg.rtr = false;
    msg.length = 2; // 2 bytes for 16-bit value (RPM or VOLT)

    uint16_t value;

    if (slave_id == 1)
    {
        // SLAVE1: Convert ADC directly to RPM
        value = (uint16_t)((adc_val * MOTOR_MAX_RPM) / ADC_MAX_VALUE);
        msg.id = CMD_SETPOINT_SLAVE1;
    }
    else if (slave_id == 2)
    {
        // SLAVE2: Convert ADC directly to VOLT
        value = (uint16_t)((adc_val * BUCK_MAX_VOLT) / ADC_MAX_VALUE);
        msg.id = CMD_SETPOINT_SLAVE2;
    }
    else
    {
        return; // Invalid slave_id
    }

    // Send value as MSB, LSB
    msg.data[0] = (uint8_t)(value >> 8);   // MSB
    msg.data[1] = (uint8_t)(value & 0xFF); // LSB

    can_transmit(&msg);
}

void can_send_setpoint_all(uint16_t adc_val)
{
    can_message_t msg;
    msg.id = CMD_SETPOINT_ALL;
    msg.rtr = false;
    msg.length = 4; // 4 bytes: 2 for RPM, 2 for VOLT

    // Calculate RPM value for SLAVE1 (first 2 bytes)
    uint16_t rpm_value = (uint16_t)((adc_val * MOTOR_MAX_RPM) / ADC_MAX_VALUE);
    msg.data[0] = (uint8_t)(rpm_value >> 8);   // RPM MSB
    msg.data[1] = (uint8_t)(rpm_value & 0xFF); // RPM LSB

    // Calculate VOLT value for SLAVE2 (last 2 bytes)
    uint16_t volt_value = (uint16_t)((adc_val * BUCK_MAX_VOLT) / ADC_MAX_VALUE);
    msg.data[2] = (uint8_t)(volt_value >> 8);   // VOLT MSB
    msg.data[3] = (uint8_t)(volt_value & 0xFF); // VOLT LSB

    can_transmit(&msg);
}

void send_setpoint_to_slaves(void)
{
    // Get current ADC value based on mode
    uint16_t adc_val = (master_ctrl.current_mode == MODE_SETPOINT_ADC) ? master_ctrl.adc_value : ((master_ctrl.uart_setpoint * ADC_MAX_VALUE) / PERCENTAGE_MAX);

    // Send setpoint based on current control target
    switch (master_ctrl.control_target)
    {
    case CONTROL_TARGET_SLAVE1:
        can_send_setpoint(1, adc_val);
        break;

    case CONTROL_TARGET_SLAVE2:
        can_send_setpoint(2, adc_val);
        break;

    case CONTROL_TARGET_ALL:
        can_send_setpoint_all(adc_val);
        break;
    }
}

void send_setpoint_to_running_slaves(void)
{
    // Get current ADC value based on mode
    uint16_t adc_val = (master_ctrl.current_mode == MODE_SETPOINT_ADC) ? master_ctrl.adc_value : ((master_ctrl.uart_setpoint * ADC_MAX_VALUE) / PERCENTAGE_MAX);

    switch (master_ctrl.control_target)
    {
    case CONTROL_TARGET_SLAVE1:
        if (slave1_info.status == STATUS_RUNNING)
        {
            can_send_setpoint(1, adc_val);
        }
        break;

    case CONTROL_TARGET_SLAVE2:
        if (slave2_info.status == STATUS_RUNNING)
        {
            can_send_setpoint(2, adc_val);
        }
        break;

    case CONTROL_TARGET_ALL:
        if (slave1_info.status == STATUS_RUNNING && slave2_info.status == STATUS_RUNNING)
        {
            can_send_setpoint_all(adc_val);
        }
        else
        {
            if (slave1_info.status == STATUS_RUNNING)
            {
                can_send_setpoint(1, adc_val);
            }
            if (slave2_info.status == STATUS_RUNNING)
            {
                can_send_setpoint(2, adc_val);
            }
        }
        break;
    }
}

void can_send_status_all(void)
{
    can_message_t msg;
    msg.id = CMD_STATUS_ALL;
    msg.rtr = true; // Remote Transmission Request
    msg.length = 0; // RTR frames have no data

    can_transmit(&msg);
}

void send_setpoint_to_target(uint8_t value)
{
    // TODO: Implementation
}

void can_rx_callback(can_message_t *msg, uint8_t fmi)
{
    // Use FMI (Filter Match Index) to identify which slave sent the message
    // FMI 0: FB_SLAVE1 (0x301)
    // FMI 1: FB_SLAVE2 (0x302)

    if (fmi == 0)
    {
        // Message from SLAVE1
        // Message format: [status, setpoint_MSB, setpoint_LSB, ...]
        if (msg->length >= 3)
        {
            slave1_info.status = msg->data[0];
            // Combine MSB and LSB to form 16-bit setpoint
            slave1_info.current_setpoint = (msg->data[1] << 8) | msg->data[2];
        }
    }
    else if (fmi == 4)
    {
        // Message from SLAVE2
        // Message format: [status, setpoint_MSB, setpoint_LSB, ...]
        if (msg->length >= 3)
        {
            slave2_info.status = msg->data[0];
            // Combine MSB and LSB to form 16-bit setpoint
            slave2_info.current_setpoint = (msg->data[1] << 8) | msg->data[2];
        }
    }

    // Set CAN RX flag for processing in main loop
    SR |= SR_CAN_RX;
}

// =================== SLAVE MANAGEMENT ===================
void slave_info_init(slave_info_t *slave, uint8_t id)
{
    slave->status = 0;
    slave->current_setpoint = 0;
}

void slave_update_connection(slave_info_t *slave)
{
    // TODO: Implementation
}

// =================== LCD DISPLAY ===================
void lcd_display_startup(void)
{
    // Hien thi man hinh khoi dong
    LCD_Clear();
    LCD_SetCursor(0, 3);
    LCD_PrintString("VI DIEU KHIEN");
    LCD_SetCursor(1, 6);
    LCD_PrintString("NANG CAO");
    LCD_SetCursor(2, 0);
    LCD_PrintString("Nhom: 2 - 21.33");
    LCD_SetCursor(3, 0);
    LCD_PrintString("Project: CAN - BUCK");
    Delay_ms(2000);

    LCD_Clear();
    LCD_SetCursor(0, 0);
    LCD_PrintString("MOTOR:     BUCK:");
    LCD_SetCursor(1, 0);
    LCD_PrintString("RPM:       VOLT:");
    LCD_SetCursor(2, 0);
    LCD_PrintString("SP:        SP:");
    LCD_SetCursor(3, 0);
    LCD_PrintString("UART:      ADC:");
}

void lcd_update_display(void)
{
    char buffer[21];

    // Display slave status on line 1 (MOTOR and BUCK status)
    // MOTOR status (SLAVE1) - Line 1, column 7
    LCD_SetCursor(0, 6);
    if (slave1_info.status == 0x00)
    {
        snprintf(buffer, sizeof(buffer), " -- ");
    }
    else if (slave1_info.status == 0x01)
    {
        snprintf(buffer, sizeof(buffer), "RUN ");
    }
    else if (slave1_info.status == 0x02)
    {
        snprintf(buffer, sizeof(buffer), "STOP");
    }
    else if (slave1_info.status == 0xFF)
    {
        snprintf(buffer, sizeof(buffer), "ERR ");
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "??? ");
    }
    LCD_PrintString(buffer);

    // BUCK status (SLAVE2) - Line 1, column 16
    LCD_SetCursor(0, 16);
    if (slave2_info.status == 0x00)
    {
        snprintf(buffer, sizeof(buffer), " -- ");
    }
    else if (slave2_info.status == 0x01)
    {
        snprintf(buffer, sizeof(buffer), "RUN ");
    }
    else if (slave2_info.status == 0x02)
    {
        snprintf(buffer, sizeof(buffer), "STOP");
    }
    else if (slave2_info.status == 0xFF)
    {
        snprintf(buffer, sizeof(buffer), "ERR ");
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "??? ");
    }
    LCD_PrintString(buffer);

    // Display SLAVE1 current setpoint (RPM feedback) - Line 2, column 5
    LCD_SetCursor(1, 5);
    if (slave1_info.current_setpoint == 0 && slave1_info.status == 0x00)
    {
        snprintf(buffer, sizeof(buffer), "---");
    }
    else
    {
        // Display as RPM value (slave sends back actual value)
        snprintf(buffer, sizeof(buffer), "%4d", slave1_info.current_setpoint);
    }
    LCD_PrintString(buffer);

    // Display SLAVE2 current setpoint (VOLT feedback) - Line 2, column 16
    LCD_SetCursor(1, 16);
    if (slave2_info.current_setpoint == 0 && slave2_info.status == 0x00)
    {
        snprintf(buffer, sizeof(buffer), "---");
    }
    else
    {
        // Display as voltage (slave sends back value in 0.01V units, so divide by 100)
        uint16_t volt_int = slave2_info.current_setpoint / 100;
        uint16_t volt_dec = (slave2_info.current_setpoint / 10) % 10;
        snprintf(buffer, sizeof(buffer), "%2d.%01d", volt_int, volt_dec);
    }
    LCD_PrintString(buffer);

    // Always update UART % display with * indicator if in UART mode (Line 4, column 5-8)
    LCD_SetCursor(3, 5);
    if (master_ctrl.current_mode == MODE_SETPOINT_UART)
    {
        snprintf(buffer, sizeof(buffer), "%3d *", master_ctrl.uart_setpoint);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%3d  ", master_ctrl.uart_setpoint);
    }
    LCD_PrintString(buffer);

    // Always update ADC % display with * indicator if in ADC mode (Line 4, column 15-19)
    LCD_SetCursor(3, 15);
    if (master_ctrl.current_mode == MODE_SETPOINT_ADC)
    {
        snprintf(buffer, sizeof(buffer), "%3d *", master_ctrl.adc_setpoint);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%3d  ", master_ctrl.adc_setpoint);
    }
    LCD_PrintString(buffer);

    // Get current setpoint percentage
    uint8_t current_setpoint = (master_ctrl.current_mode == MODE_SETPOINT_ADC) ? master_ctrl.adc_setpoint : master_ctrl.uart_setpoint;

    // Calculate RPM for display: 0-100% -> 0-6200 RPM
    static uint16_t display_rpm = 0;

    // Update RPM Setpoint with * indicator only if SLAVE1 is being controlled
    LCD_SetCursor(2, 4);
    if (master_ctrl.control_target == CONTROL_TARGET_SLAVE1)
    {
        // Only SLAVE1 controlled - show * only for SLAVE1
        display_rpm = (uint16_t)((current_setpoint * MOTOR_MAX_RPM) / PERCENTAGE_MAX);
        snprintf(buffer, sizeof(buffer), "%4d *", display_rpm);
    }
    else if (master_ctrl.control_target == CONTROL_TARGET_ALL)
    {
        // Both slaves controlled - show * for both
        display_rpm = (uint16_t)((current_setpoint * MOTOR_MAX_RPM) / PERCENTAGE_MAX);
        snprintf(buffer, sizeof(buffer), "%4d *", display_rpm);
    }
    else
    {
        // SLAVE2 only controlled - no * for SLAVE1
        snprintf(buffer, sizeof(buffer), "%4d  ", display_rpm);
    }
    LCD_PrintString(buffer);

    // Calculate Volt for display: 0-100% -> 0-10.00V (stored as 0-1000)
    static uint16_t display_volt = 0;

    // Update Volt Setpoint with * indicator only if SLAVE2 is being controlled
    LCD_SetCursor(2, 14);
    if (master_ctrl.control_target == CONTROL_TARGET_SLAVE2)
    {
        // Only SLAVE2 controlled - show * only for SLAVE2
        display_volt = (uint16_t)((current_setpoint * BUCK_MAX_VOLT) / PERCENTAGE_MAX);
        uint16_t volt_int = display_volt / 100;       // Integer part
        uint16_t volt_dec = (display_volt / 10) % 10; // Decimal part
        snprintf(buffer, sizeof(buffer), "%2d.%01d *", volt_int, volt_dec);
    }
    else if (master_ctrl.control_target == CONTROL_TARGET_ALL)
    {
        // Both slaves controlled - show * for both
        display_volt = (uint16_t)((current_setpoint * BUCK_MAX_VOLT) / PERCENTAGE_MAX);
        uint16_t volt_int = display_volt / 100;       // Integer part
        uint16_t volt_dec = (display_volt / 10) % 10; // Decimal part
        snprintf(buffer, sizeof(buffer), "%2d.%01d *", volt_int, volt_dec);
    }
    else
    {
        // SLAVE1 only controlled - no * for SLAVE2
        uint16_t volt_int = display_volt / 100;       // Integer part
        uint16_t volt_dec = (display_volt / 10) % 10; // Decimal part
        snprintf(buffer, sizeof(buffer), "%2d.%01d  ", volt_int, volt_dec);
    }
    LCD_PrintString(buffer);
}

void lcd_display_mode_setpoint_uart(void)
{
    // TODO: Implementation
}

void lcd_display_mode_setpoint_adc(void)
{
    // TODO: Implementation
}

// =================== TIMER CALLBACK ===================
void Timer4_IRQ_Callback(void)
{
    lcd_update_counter++;
    status_check_counter++;

    if (lcd_update_counter >= LCD_UPDATE_PERIOD_MS)
    {
        lcd_update_counter = 0;
        SR |= SR_TIMER_100MS;     // Set 100ms timer flag
        SR |= SR_LCD_UPDATE;      // Set LCD update flag
        SR |= SR_ADC_UPDATE;      // Set ADC update flag
        SR |= SR_SETPOINT_UPDATE; // Set setpoint update flag
    }

    // Status check every 1 seconds (1000ms)
    if (status_check_counter >= 1000)
    {
        status_check_counter = 0;
        SR |= SR_STATUS_CHECK; // Set status check flag
    }
}

// =================== INTERRUPT HANDLERS ===================
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR0)
    {
        EXTI->PR = EXTI_PR_PR0; // Clear pending bit
        SR |= SR_BTN_SLAVE1;    // Set button slave1 flag in SR (PB0)
    }
}

void EXTI1_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR1)
    {
        EXTI->PR = EXTI_PR_PR1; // Clear pending bit
        SR |= SR_BTN_SLAVE2;    // Set button slave2 flag in SR (PB1)
    }
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI->PR & EXTI_PR_PR10)
    {
        EXTI->PR = EXTI_PR_PR10; // Clear pending bit
        SR |= SR_BTN_MODE;       // Set button mode flag in SR (PB10)
    }

    if (EXTI->PR & EXTI_PR_PR11)
    {
        EXTI->PR = EXTI_PR_PR11; // Clear pending bit
        SR |= SR_BTN_CONTROL;    // Set button control flag in SR (PB11)
    }
}

// =================== UART DMA INTERRUPT HANDLERS ===================
void USART1_IRQHandler(void)
{
    if (USART1->SR & (1 << 4)) // Check IDLE flag
    {
        // Clear IDLE flag by reading SR then DR
        volatile uint32_t dummy;
        dummy = USART1->SR;
        dummy = USART1->DR;
        (void)dummy; // Prevent unused variable warning
        
        // Calculate number of bytes received
        uart_rx_length = UART_RX_BUFFER_SIZE - DMA1_Channel5->CNDTR;
        
        if (uart_rx_length > 0)
        {
            // Set flag for main loop to process
            uart_received = 1;
            
            // Reset DMA for next reception
            DMA1_Channel5->CCR &= ~(1 << 0);                        // Disable channel
            DMA1_Channel5->CNDTR = UART_RX_BUFFER_SIZE;             // Reset counter
            DMA1_Channel5->CCR |= (1 << 0);                         // Enable channel
        }
    }
}

void DMA1_Channel5_IRQHandler(void) // USART1 RX
{
    if (DMA1->ISR & (1 << 17))
    {                              // TCIF5: Transfer Complete (buffer full)
        DMA1->IFCR |= (1 << 17);   // Clear flag
        
        // Buffer is full
        uart_rx_length = UART_RX_BUFFER_SIZE;
        uart_received = 1;         // Set received flag
        
        // Reset DMA for next reception
        DMA1_Channel5->CCR &= ~(1 << 0);                        // Disable channel
        DMA1_Channel5->CNDTR = UART_RX_BUFFER_SIZE;             // Reset counter
        DMA1_Channel5->CCR |= (1 << 0);                         // Enable channel
    }
}

void DMA1_Channel4_IRQHandler(void) // USART1 TX
{
    if (DMA1->ISR & (1 << 13))
    {                                    // TCIF4: Transfer Complete
        DMA1->IFCR |= (1 << 13);         // Clear flag
        DMA1_Channel4->CCR &= ~(1 << 0); // Disable channel
        uart1_tx_complete = 1;
    }
}