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

// ============================================================================
// PUBLIC FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Khoi tao CAN1 cho Standard CAN 2.0A
 * @param filter_id: Standard ID can nhan, 0 = chap nhan tat ca
 */
void can_init(uint16_t filter_id);

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
 * @brief Callback khi nhan duoc message - INTERRUPT
 * @param msg: Con tro den message vua nhan
 * @note Ham yeu - nguoi dung co the override trong main.c
 */
__attribute__((weak)) void can_rx_callback(can_message_t *msg);

#endif // CAN_H