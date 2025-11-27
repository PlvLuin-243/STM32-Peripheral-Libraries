#ifndef CAN_H
#define CAN_H

#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// STANDARD CAN 2.0A CONFIGURATION
// ============================================================================
#define CAN_STANDARD_DATA_LENGTH    8       // Standard CAN co dinh 8 bytes
#define CAN_MAX_DLC                 8       // Data Length Code toi da = 8
#define CAN_STANDARD_ID_MASK        0x7FF   // 11-bit Standard ID mask

// ============================================================================
// DATA STRUCTURES
// ============================================================================
typedef struct {
    uint16_t id;                                    // Standard ID (11-bit) - 0x000 to 0x7FF
    bool rtr;                                      // false = Data frame, true = Remote frame
    uint8_t length;                                // DLC (0-8 bytes)
    uint8_t data[CAN_STANDARD_DATA_LENGTH];        // Du lieu co dinh 8 bytes
} can_message_t;

typedef enum {
    // Bank 0: Mask mode 0x100-0x10F (STATUS and SETPOINT_ALL)
    CMD_SETPOINT_ALL = 0x100,
    CMD_STATUS_ALL = 0x101,
    CMD_STATUS_SLAVE1 = 0x102,
    CMD_STATUS_SLAVE2 = 0x103,
    
    // Bank 1: List mode (START/STOP commands)
    CMD_START_SLAVE1 = 0x120,
    CMD_STOP_SLAVE1 = 0x121,
    CMD_START_SLAVE2 = 0x130,
    CMD_STOP_SLAVE2 = 0x131,
    
    // Bank 2: List mode (SETPOINT commands)
    CMD_SETPOINT_SLAVE1 = 0x122,
    CMD_SETPOINT_SLAVE2 = 0x132
} can_command_t;

typedef enum {
    FB_SLAVE1 = 0x301,
    FB_SLAVE2 = 0x302,
}can_feedback_t;

// ============================================================================
// PUBLIC FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Khoi tao CAN1 cho Standard CAN 2.0A
 */
void can_init(void);

/**
 * @brief Gui CAN message - NON-BLOCKING
 * @param msg: Con tro den message can gui
 * @return true neu gui thanh cong, false neu that bai
 */
bool can_transmit(can_message_t *msg);

/**
 * @brief Nhan CAN message - BLOCKING
 * @param msg: Con tro den buffer de luu message
 * @param timeout: Thoi gian timeout (ms)
 * @return true neu nhan thanh cong, false neu timeout
 */
bool can_receive(can_message_t *msg, uint32_t timeout);

/**
 * @brief Kiem tra Standard ID hop le
 * @param id: ID can kiem tra
 * @return true neu hop le, false neu khong hop le
 */
bool can_is_valid_standard_id(uint16_t id);

/**
 * @brief Lay bus error count
 * @return So loi bus hien tai
 */
uint8_t can_get_bus_error_count(void);

/**
 * @brief Cấu hình bộ lọc CAN MASTER
 * @param fb_slave1: Feedback ID của Slave 1 (FB_SLAVE1 = 0x301)
 * @param fb_slave2: Feedback ID của Slave 2 (FB_SLAVE2 = 0x302)
 * @note Bank 0: Lọc FB_SLAVE1 (0x301)
 * @note Bank 1: Lọc FB_SLAVE2 (0x302)
 */
void can_master_setup_filters(can_feedback_t fb_slave1, can_feedback_t fb_slave2);

/**
 * @brief Cấu hình bộ lọc CAN SLAVE với 3 filter banks
 * @param cmd_start: Command START (CMD_START_SLAVE1 hoặc CMD_START_SLAVE2)
 * @param cmd_stop: Command STOP (CMD_STOP_SLAVE1 hoặc CMD_STOP_SLAVE2)
 * @param cmd_setpoint: Command SETPOINT riêng (CMD_SETPOINT_SLAVE1 hoặc CMD_SETPOINT_SLAVE2)
 * @param cmd_status: Command STATUS riêng (CMD_STATUS_SLAVE1 hoặc CMD_STATUS_SLAVE2)
 * 
 * @note Bank 0: Mask mode - Chấp nhận 0x100-0x10F (STATUS và SETPOINT_ALL)
 * @note Bank 1: List mode - START và STOP commands
 * @note Bank 2: List mode - SETPOINT command riêng của slave
 */
void can_slave_setup_filters(can_command_t cmd_start, can_command_t cmd_stop, can_command_t cmd_setpoint, can_command_t cmd_status);

/**
 * @brief Callback khi nhan duoc message - INTERRUPT
 * @param msg: Con tro den message vua nhan
 * @note Ham yeu - nguoi dung co the override trong main.c
 */
__attribute__((weak)) void can_rx_callback(can_message_t *msg, uint8_t fmi);

#endif // CAN_H