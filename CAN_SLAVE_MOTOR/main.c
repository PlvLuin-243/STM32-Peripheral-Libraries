#include "stm32f10x.h"
#include "gpio.h"
#include "can.h"
#include "systick.h"
#include "timer.h"
#include "pid.h"
#include <stdbool.h>

typedef enum {
    STATUS_IDLE = 0x00,
    STATUS_RUNNING = 0x01,
    STATUS_STOPPED = 0x02,
    STATUS_SETPOINT_OK = 0x03,
    STATUS_ERROR = 0xFF
} slave_status_t;

#define SLAVE_ID 1
#define SLAVE_TYPE_MOTOR  1
#define SLAVE_TYPE_BUCK   2
#define SLAVE_TYPE        SLAVE_TYPE_MOTOR
#define MAX_RPM             6200
#define ENCODER_PPR         96
#define PID_OUTPUT_MAX      9999.0f
#define PID_OUTPUT_MIN      0.0f
#define SETPOINT_RAMP_STEP  5   // Ramping bình thường: 5 RPM/ms = 5000 RPM/s
#define STOP_DECEL_STEP     1   // Giảm chậm khi STOP: 1 RPM/ms = 1000 RPM/s

volatile bool system_running = false;
volatile bool stop_mode = false;        // Chế độ stop đặc biệt (giảm chậm)
volatile uint16_t target_setpoint = 0;   // Setpoint mục tiêu từ CAN
volatile uint16_t current_setpoint = 0;  // Setpoint thực tế sau ramping
volatile uint8_t slave_status = STATUS_IDLE;
volatile uint16_t current_rpm = 0;
PIDController motor_pid;
volatile uint8_t mode_update_flag = 0;
volatile uint16_t stop_feedback_timer = 0;  // Bộ đếm thời gian cho feedback khi stop (đơn vị: ms)

void led_init(void);
void motor_control_init(void);
void motor_control_task(void);
void send_feedback(void);
void handle_start_command(void);
void handle_stop_command(void);
void handle_setpoint_command(can_message_t *msg);
void handle_setpoint_all_command(can_message_t *msg);
void Timer2_IRQ_Callback(void);

int main(void)
{
    SysTick_Init();
    led_init();
    motor_control_init();
    can_init();
    can_slave_setup_filters(CMD_START_SLAVE1, CMD_STOP_SLAVE1, CMD_SETPOINT_SLAVE1, CMD_STATUS_SLAVE1);
    slave_status = STATUS_STOPPED;
    
    while(1)
    {
        if(mode_update_flag)
        {
            motor_control_task();
            mode_update_flag = 0;
        }
    }
}

void led_init(void)
{
    gpio_init_output(GPIOC, 13);
    gpio_write_pin(GPIOC, 13, HIGH);
}

void motor_control_init(void)
{
    TIM1_Input_Capture_Init(ENCODER_PPR);
    Timer_PWM_Init(TIM3, PWM_CHANNEL_1, 0, 9999);
    Timer_IRQ_Init(TIM2, 71, 999);  // 100us interrupt for faster sampling
    PIDController_Init(&motor_pid);
    current_rpm = 0;
}

void motor_control_task(void)
{
    // Luôn cập nhật RPM để feedback CAN chính xác
    float measured_rpm = TIM1_Get_RPM_Average();
    current_rpm = (uint16_t)measured_rpm;
    
    // Nếu system không chạy, tắt PWM và LED
    if (!system_running) {
        Timer_PWM_Set_CCR_Value(TIM3, PWM_CHANNEL_1, 0);
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
        Timer_PWM_Set_CCR_Value(TIM3, PWM_CHANNEL_1, 0);
        // Reset PID state
        motor_pid.integrator = 0.0f;
        motor_pid.differentiator = 0.0f;
        motor_pid.prevError = 0.0f;
        motor_pid.prevMeasurement = 0.0f;
        return;
    }
    
    // Chạy PID với setpoint hiện tại
    float setpoint = (float)current_setpoint;
    float pid_output = PIDController_Update(&motor_pid, setpoint, measured_rpm);
    
    if(pid_output > PID_OUTPUT_MAX) pid_output = PID_OUTPUT_MAX;
    if(pid_output < PID_OUTPUT_MIN) pid_output = PID_OUTPUT_MIN;
    
    Timer_PWM_Set_CCR_Value(TIM3, PWM_CHANNEL_1, (uint16_t)pid_output);
}

void Timer2_IRQ_Callback(void)
{
    mode_update_flag = 1;
    
    // Tự động gửi feedback mỗi 100ms khi stop_feedback_timer > 0
    if (stop_feedback_timer > 0) {
        if (--stop_feedback_timer % 100 == 0) {  // Mỗi 100ms
            send_feedback();
        }
    }
}

void send_feedback(void)
{
    can_message_t msg;
    msg.id = (SLAVE_ID == 1) ? FB_SLAVE1 : FB_SLAVE2;
    msg.rtr = false;
    msg.length = 3;
    msg.data[0] = slave_status;
    
#if (SLAVE_TYPE == SLAVE_TYPE_MOTOR)
    msg.data[1] = (uint8_t)(current_rpm >> 8);
    msg.data[2] = (uint8_t)(current_rpm & 0xFF);
#elif (SLAVE_TYPE == SLAVE_TYPE_BUCK)
    msg.data[1] = (uint8_t)(current_setpoint >> 8);
    msg.data[2] = (uint8_t)(current_setpoint & 0xFF);
#else
    msg.data[1] = (uint8_t)(current_setpoint >> 8);
    msg.data[2] = (uint8_t)(current_setpoint & 0xFF);
#endif
    
    can_transmit(&msg);
}

void handle_start_command(void)
{
    system_running = true;
    slave_status = STATUS_RUNNING;
    gpio_write_pin(GPIOC, 13, LOW);
    send_feedback();
}

void handle_stop_command(void)
{
    // Bật chế độ stop đặc biệt: giảm rất chậm về 0
    stop_mode = true;
    target_setpoint = 0;
    stop_feedback_timer = 0xFFFF ;  // Kích hoạt auto-feedback
    slave_status = STATUS_STOPPED;
    gpio_write_pin(GPIOC, 13, HIGH);
    send_feedback();
}

void handle_setpoint_command(can_message_t *msg)
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
    if (target_setpoint > 1000) {
        target_setpoint = 1000;
    }
#else
    target_setpoint = value;
#endif
    
    // CHỈ chấp nhận setpoint khi system_running = true
    // PHẢI gửi START trước khi setpoint có hiệu lực
    if (!system_running) {
        // Lưu setpoint nhưng không bật system
        slave_status = STATUS_STOPPED;
        send_feedback();
        return;
    }
    
    // Tắt chế độ stop khi nhận setpoint mới (chỉ khi đang chạy)
    stop_mode = false;
    stop_feedback_timer = 0;
    
    slave_status = STATUS_RUNNING;
    send_feedback();
}

void handle_setpoint_all_command(can_message_t *msg)
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
    if (target_setpoint > 1000) {
        target_setpoint = 1000;
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
        send_feedback();
        return;
    }
    
    // Tắt chế độ stop khi nhận setpoint mới (chỉ khi đang chạy)
    stop_mode = false;
    stop_feedback_timer = 0;
    
    slave_status = STATUS_RUNNING;
    send_feedback();
}

void can_rx_callback(can_message_t *msg, uint8_t fmi)
{
    switch(msg->id) {
        case CMD_START_SLAVE1:
            handle_start_command();
            break;
            
        case CMD_STOP_SLAVE1:
            handle_stop_command();
            break;
            
        case CMD_SETPOINT_SLAVE1:
            handle_setpoint_command(msg);
            break;
            
        case CMD_SETPOINT_ALL:
            handle_setpoint_all_command(msg);
            break;
            
        case CMD_STATUS_ALL:
        case CMD_STATUS_SLAVE1:
            send_feedback();
            break;
            
        default:
            slave_status = STATUS_ERROR;
            send_feedback();
            break;
    }
}