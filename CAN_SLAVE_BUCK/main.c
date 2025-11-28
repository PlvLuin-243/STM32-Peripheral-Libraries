#include "stm32f10x.h"
#include "gpio.h"
#include "can.h"
#include "systick.h"
#include "timer.h"
#include "pid.h"
#include "adc.h"
#include <stdbool.h>

typedef enum {
    STATUS_IDLE = 0x00,
    STATUS_RUNNING = 0x01,
    STATUS_STOPPED = 0x02,
    STATUS_SETPOINT_OK = 0x03,
    STATUS_ERROR = 0xFF
} slave_status_t;

#define SLAVE_ID 2
#define SLAVE_TYPE_MOTOR  1
#define SLAVE_TYPE_BUCK   2
#define SLAVE_TYPE        SLAVE_TYPE_BUCK
#define MAX_RPM             6000
#define ENCODER_PPR         96
#define PID_OUTPUT_MAX      9999.0f
#define PID_OUTPUT_MIN      0.0f
#define SETPOINT_RAMP_STEP  5   // Ramping bình thường: 5 RPM/ms = 5000 RPM/s
#define STOP_DECEL_STEP     1   // Giảm chậm khi STOP: 1 RPM/ms = 1000 RPM/s

// PID Constants for Motor
#define PID_MOTOR_KP        2.0f
#define PID_MOTOR_KI        6.0f
#define PID_MOTOR_KD        0.00001f
#define PID_MOTOR_TAU       0.01f
#define PID_MOTOR_SAMPLE_T  0.001f

// PID Constants for Buck Converter
#define PID_BUCK_KP         2.0f
#define PID_BUCK_KI         6.0f
#define PID_BUCK_KD         0.00001f
#define PID_BUCK_TAU        0.01f
#define PID_BUCK_SAMPLE_T   0.001f

// Voltage Conversion Constants
#define VOLTAGE_SCALE_FACTOR    490.0f
#define ADC_RESOLUTION          4095.0f
#define ADC_VREF                3.3f
#define BUCK_MAX_SETPOINT       1000
#define BUCK_MAX_FEEDBACK       900   // 9V maximum feedback (900 = 9.00V)

volatile bool system_running = false;
volatile bool stop_mode = false;        // Chế độ stop đặc biệt (giảm chậm)
volatile uint16_t target_setpoint = 0;   // Setpoint mục tiêu từ CAN
volatile uint16_t current_setpoint = 0;  // Setpoint thực tế sau ramping
volatile uint8_t slave_status = STATUS_IDLE;
volatile uint16_t current_rpm = 0;
volatile uint16_t adc_reading = 0;  // ADC value for Buck converter
PIDController motor_pid;
volatile uint8_t mode_update_flag = 0;
volatile uint16_t stop_feedback_timer = 0;  // Bộ đếm thời gian cho feedback khi stop (đơn vị: ms)

void system_led_init(void);
void slave_control_init(void);
void slave_control_task(void);
void slave_send_feedback(void);
void slave_handle_start_command(void);
void slave_handle_stop_command(void);
void slave_handle_setpoint_command(can_message_t *msg);
void slave_handle_setpoint_all_command(can_message_t *msg);
void Timer2_IRQ_Callback(void);

int main(void)
{
    SysTick_Init();
    system_led_init();
    slave_control_init();
    can_init();
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // Cấu hình bộ lọc cho SLAVE1 (Motor)
    can_slave_setup_filters(CMD_START_SLAVE1, CMD_STOP_SLAVE1, CMD_SETPOINT_SLAVE1, CMD_STATUS_SLAVE1);
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // Cấu hình bộ lọc cho SLAVE2 (Buck Converter)
    can_slave_setup_filters(CMD_START_SLAVE2, CMD_STOP_SLAVE2, CMD_SETPOINT_SLAVE2, CMD_STATUS_SLAVE2);
#endif
    
    slave_status = STATUS_STOPPED;
    
    while(1)
    {
        if(mode_update_flag)
        {
            slave_control_task();
            mode_update_flag = 0;
        }
    }
}

void system_led_init(void)
{
    gpio_init_output(GPIOC, 13);
    gpio_write_pin(GPIOC, 13, HIGH);
}

void slave_control_init(void)
{
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    TIM1_Input_Capture_Init(ENCODER_PPR);
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // Cấu hình ADC cho Buck Converter (PA0 = ADC Channel 0)
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;  // Enable GPIOA clock
    GPIOA->CRL &= ~(0xF << (0 * 4));     // PA0: Analog mode (0000)
    DMA1_Channel1_Config();
    ADC1_DMA_Config(0);  // Channel 0 for PA0
#endif
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // Motor: PWM trên TIM3 Channel 1 (PA6) - Normal polarity
    Timer_PWM_Init(TIM3, PWM_CHANNEL_1, 0, 9999);
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // Buck Converter: PWM trên TIM1 Channel 1 (PA8) - Inverted polarity
    Timer_PWM_Init(TIM1, PWM_CHANNEL_1, 0, 9999);
    // Đảo polarity của TIM1 Channel 1: CC1P = 1 (Active Low)
    TIM1->CCER |= (1 << 1);  // CC1P bit = 1: Inverted polarity
#endif
    
    Timer_IRQ_Init(TIM2, 71, 999);  // 1ms interrupt for faster sampling
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // Cấu hình PID cho MOTOR (Slave 1)
    motor_pid.Kp = PID_MOTOR_KP;
    motor_pid.Ki = PID_MOTOR_KI;
    motor_pid.Kd = PID_MOTOR_KD;
    motor_pid.tau = PID_MOTOR_TAU;
    motor_pid.limMin = PID_OUTPUT_MIN;
    motor_pid.limMax = PID_OUTPUT_MAX;
    motor_pid.limMinInt = PID_OUTPUT_MIN;
    motor_pid.limMaxInt = PID_OUTPUT_MAX;
    motor_pid.T = PID_MOTOR_SAMPLE_T;
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // Cấu hình PID cho BUCK CONVERTER (Slave 2)
    motor_pid.Kp = PID_BUCK_KP;
    motor_pid.Ki = PID_BUCK_KI;
    motor_pid.Kd = PID_BUCK_KD;
    motor_pid.tau = PID_BUCK_TAU;
    motor_pid.limMin = PID_OUTPUT_MIN;
    motor_pid.limMax = PID_OUTPUT_MAX;
    motor_pid.limMinInt = PID_OUTPUT_MIN;
    motor_pid.limMaxInt = PID_OUTPUT_MAX;
    motor_pid.T = PID_BUCK_SAMPLE_T;
#endif
    PIDController_Init(&motor_pid);
    
    current_rpm = 0;
}

void slave_control_task(void)
{
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // Cập nhật RPM cho Motor
    float measured_rpm = TIM1_Get_RPM_Average();
    current_rpm = (uint16_t)measured_rpm;
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // Đọc ADC cho Buck Converter (PA0)
    adc_reading = adc_value;  // Lấy giá trị từ DMA
    current_rpm = adc_reading;  // Sử dụng current_rpm để lưu ADC value
#endif
    
    // Nếu system không chạy, tắt PWM và LED
    if (!system_running) {
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
        Timer_PWM_Set_CCR_Value(TIM3, PWM_CHANNEL_1, 0);  // Motor: TIM3
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
        Timer_PWM_Set_CCR_Value(TIM1, PWM_CHANNEL_1, 0);  // Buck: TIM1
#endif
        gpio_write_pin(GPIOC, 13, HIGH);
        return;
    }
    
    // Chỉ sử dụng ramping chậm khi ở chế độ stop
    if (stop_mode) {
        // Giảm chậm current_setpoint về 0
        if (current_setpoint > STOP_DECEL_STEP) {
            current_setpoint -= STOP_DECEL_STEP;
        } else {
            current_setpoint = 0;
            stop_mode = false;
            stop_feedback_timer = 1000;  // Gửi feedback thêm 1 giây nữa
        }
    } else {
        // Ramping bình thường khi thay đổi setpoint
        if (current_setpoint < target_setpoint) {
            // Tăng dần
            if (target_setpoint - current_setpoint > SETPOINT_RAMP_STEP) {
                current_setpoint += SETPOINT_RAMP_STEP;
            } else {
                current_setpoint = target_setpoint;
            }
        } else if (current_setpoint > target_setpoint) {
            // Giảm dần
            if (current_setpoint - target_setpoint > SETPOINT_RAMP_STEP) {
                current_setpoint -= SETPOINT_RAMP_STEP;
            } else {
                current_setpoint = target_setpoint;
            }
        }
    }
    
    // Nếu setpoint = 0, tắt PWM và reset PID
    if (current_setpoint == 0) {
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
        Timer_PWM_Set_CCR_Value(TIM3, PWM_CHANNEL_1, 0);  // Motor: TIM3
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
        Timer_PWM_Set_CCR_Value(TIM1, PWM_CHANNEL_1, 0);  // Buck: TIM1
#endif
        // Reset PID state
        motor_pid.integrator = 0.0f;
        motor_pid.differentiator = 0.0f;
        motor_pid.prevError = 0.0f;
        motor_pid.prevMeasurement = 0.0f;
        return;
    }
    
    // Chạy PID với setpoint hiện tại
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    float setpoint = (float)current_setpoint;  // Motor: setpoint trực tiếp là RPM
    float feedback = measured_rpm;  // Motor: dùng RPM feedback
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // Buck: Chuyển đổi setpoint từ giá trị voltage sang ADC tương ứng
    // Công thức: setpoint_value / VOLTAGE_SCALE_FACTOR * (ADC_RESOLUTION/ADC_VREF)
    float setpoint = ((float)current_setpoint / VOLTAGE_SCALE_FACTOR) * (ADC_RESOLUTION / ADC_VREF);
    float feedback = (float)adc_reading;  // Buck: dùng ADC feedback
#endif
    
    float pid_output = PIDController_Update(&motor_pid, setpoint, feedback);
    
    if(pid_output > PID_OUTPUT_MAX) pid_output = PID_OUTPUT_MAX;
    if(pid_output < PID_OUTPUT_MIN) pid_output = PID_OUTPUT_MIN;
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    Timer_PWM_Set_CCR_Value(TIM3, PWM_CHANNEL_1, (uint16_t)pid_output);  // Motor: TIM3 PA6 - Normal PWM
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    Timer_PWM_Set_CCR_Value(TIM1, PWM_CHANNEL_1, (uint16_t)pid_output);  // Buck: TIM1 PA8 - Hardware Inverted PWM
#endif
}

void Timer2_IRQ_Callback(void)
{
    mode_update_flag = 1;
    
    // Tự động gửi feedback mỗi 100ms khi stop_feedback_timer > 0
    if (stop_feedback_timer > 0) {
        if (--stop_feedback_timer % 100 == 0) {  // Mỗi 100ms
            slave_send_feedback();
        }
    }
}

void slave_send_feedback(void)
{
    can_message_t msg;
    msg.rtr = false;
    msg.length = 3;
    msg.data[0] = slave_status;
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // SLAVE1 (Motor): Gửi feedback với RPM thực tế
    msg.id = FB_SLAVE1;
    msg.data[1] = (uint8_t)(current_rpm >> 8);
    msg.data[2] = (uint8_t)(current_rpm & 0xFF);
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // SLAVE2 (Buck): Chuyển đổi ADC reading về giá trị voltage
    // Công thức ngược: adc_value * ADC_VREF / ADC_RESOLUTION * VOLTAGE_SCALE_FACTOR
    uint16_t voltage_feedback = (uint16_t)((float)adc_reading * ADC_VREF / ADC_RESOLUTION * VOLTAGE_SCALE_FACTOR);
    
    // Giới hạn feedback tối đa 9V (900)
    if (voltage_feedback > BUCK_MAX_FEEDBACK) {
        voltage_feedback = BUCK_MAX_FEEDBACK;
    }
    
    msg.id = FB_SLAVE2;
    msg.data[1] = (uint8_t)(voltage_feedback >> 8);
    msg.data[2] = (uint8_t)(voltage_feedback & 0xFF);
#endif
    
    can_transmit(&msg);
}

void slave_handle_start_command(void)
{
    system_running = true;
    slave_status = STATUS_RUNNING;
    gpio_write_pin(GPIOC, 13, LOW);
    slave_send_feedback();
}

void slave_handle_stop_command(void)
{
    // Bật chế độ stop đặc biệt: giảm rất chậm về 0
    stop_mode = true;
    target_setpoint = 0;
    stop_feedback_timer = 0xFFFF ;  // Kích hoạt auto-feedback
    slave_status = STATUS_STOPPED;
    gpio_write_pin(GPIOC, 13, HIGH);
    slave_send_feedback();
}

void slave_handle_setpoint_command(can_message_t *msg)
{
    if (msg->length < 2) {
        return;
    }
    
    // Nhận 2 bytes: MSB và LSB
    uint16_t value = (msg->data[0] << 8) | msg->data[1];
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // SLAVE1: Nhận trực tiếp giá trị RPM (0-6200)
    target_setpoint = value;
    if (target_setpoint > MAX_RPM) {
        target_setpoint = MAX_RPM;
    }
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // SLAVE2: Nhận trực tiếp giá trị VOLT (0-1000 = 0-10.00V)
    target_setpoint = value;
    if (target_setpoint > BUCK_MAX_SETPOINT) {
        target_setpoint = BUCK_MAX_SETPOINT;
    }
#else
    target_setpoint = value;
#endif
    
    // CHỈ chấp nhận setpoint khi system_running = true
    // PHẢI gửi START trước khi setpoint có hiệu lực
    if (!system_running) {
        // Lưu setpoint nhưng không bật system
        slave_status = STATUS_STOPPED;
        slave_send_feedback();
        return;
    }
    
    // Tắt chế độ stop khi nhận setpoint mới (chỉ khi đang chạy)
    stop_mode = false;
    stop_feedback_timer = 0;
    
    slave_status = STATUS_RUNNING;
    slave_send_feedback();
}

void slave_handle_setpoint_all_command(can_message_t *msg)
{
    if (msg->length < 4) {
        return;  // Cần ít nhất 4 bytes cho cả 2 slave
    }
    
    uint16_t value;
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // SLAVE1 (MOTOR): Lấy 2 byte đầu (byte 0-1)
    value = (msg->data[0] << 8) | msg->data[1];
    target_setpoint = value;
    if (target_setpoint > MAX_RPM) {
        target_setpoint = MAX_RPM;
    }
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // SLAVE2 (BUCK): Lấy 2 byte sau (byte 2-3)
    value = (msg->data[2] << 8) | msg->data[3];
    target_setpoint = value;
    if (target_setpoint > BUCK_MAX_SETPOINT) {
        target_setpoint = BUCK_MAX_SETPOINT;
    }
#else
    // Default: lấy 2 byte đầu
    value = (msg->data[0] << 8) | msg->data[1];
    target_setpoint = value;
#endif
    
    // CHỈ chấp nhận setpoint khi system_running = true
    // PHẢI gửi START trước khi setpoint có hiệu lực
    if (!system_running) {
        // Lưu setpoint nhưng không bật system
        slave_status = STATUS_STOPPED;
        slave_send_feedback();
        return;
    }
    
    // Tắt chế độ stop khi nhận setpoint mới (chỉ khi đang chạy)
    stop_mode = false;
    stop_feedback_timer = 0;
    
    slave_status = STATUS_RUNNING;
    slave_send_feedback();
}

void can_rx_callback(can_message_t *msg, uint8_t fmi)
{
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    // SLAVE1 (Motor) - Xử lý các lệnh cho motor
    switch(msg->id) {
        case CMD_START_SLAVE1:
            slave_handle_start_command();
            break;
            
        case CMD_STOP_SLAVE1:
            slave_handle_stop_command();
            break;
            
        case CMD_SETPOINT_SLAVE1:
            slave_handle_setpoint_command(msg);
            break;
            
        case CMD_SETPOINT_ALL:
            slave_handle_setpoint_all_command(msg);
            break;
            
        case CMD_STATUS_ALL:
        case CMD_STATUS_SLAVE1:
            slave_send_feedback();
            break;
            
        default:
            slave_status = STATUS_ERROR;
            slave_send_feedback();
            break;
    }
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    // SLAVE2 (Buck Converter) - Xử lý các lệnh cho buck converter
    switch(msg->id) {
        case CMD_START_SLAVE2:
            slave_handle_start_command();
            break;
            
        case CMD_STOP_SLAVE2:
            slave_handle_stop_command();
            break;
            
        case CMD_SETPOINT_SLAVE2:
            slave_handle_setpoint_command(msg);
            break;
            
        case CMD_SETPOINT_ALL:
            slave_handle_setpoint_all_command(msg);
            break;
            
        case CMD_STATUS_ALL:
        case CMD_STATUS_SLAVE2:
            slave_send_feedback();
            break;
            
        default:
            slave_status = STATUS_ERROR;
            slave_send_feedback();
            break;
    }
#endif
}