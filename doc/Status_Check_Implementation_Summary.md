# CAN Master-Slave Status Check Implementation Summary

## Overview
This document describes the complete implementation of the periodic status checking system for CAN Master-Slave communication.

## Features Implemented

### 1. RTR-Based Status Checking
- **Command**: `CMD_STATUS_ALL` (0x101)
- **Type**: Remote Transmission Request (RTR) frame
- **Configuration**: `rtr=true`, `length=0` (no data payload)
- **Purpose**: Request status from all slaves simultaneously

### 2. Periodic Status Check Timer
- **Period**: 3000ms (3 seconds)
- **Implementation**: 1ms timer interrupt increments `status_check_counter`
- **Trigger**: When counter reaches 3000, sets `SR_STATUS_CHECK` flag
- **Initial Check**: Performed 100ms after startup completion

### 3. Connection Timeout Detection
- **Timeout Period**: 5000ms (5 seconds)
- **Mechanism**: Tracks `last_response_time` for each slave
- **Action**: Sets `connection_ok = false` and `status = 0` when timeout exceeded
- **Update Frequency**: Checked before every LCD update (100ms)

### 4. LCD Visual Feedback
- **Normal Operation**: Displays current setpoint value
- **Disconnected State**: Displays `"--"` when:
  - `slave_info.status == 0`, OR
  - `slave_info.connection_ok == false`
- **Display Positions**:
  - Slave1 (RPM): Line 1, Column 5
  - Slave2 (VOLT): Line 1, Column 16

## Code Structure

### Data Structures

```c
typedef struct {
    uint8_t id;                    // Slave ID
    uint8_t status;                // 0=disconnected, 1=ready, 2=running, 3=error
    uint8_t running_state;         // Running state
    uint16_t current_setpoint;     // Current setpoint value
    bool connection_ok;            // Connection status flag
    uint32_t last_response_time;   // Last received message timestamp (ms)
} slave_info_t;
```

### Status Register Flags

```c
#define SR_STATUS_CHECK      (1 << 8)  // Status check request flag (3-second timer)
```

### Key Functions

#### 1. Status Request Function
```c
void can_send_status_all(void)
{
    can_message_t msg;
    msg.id = CMD_STATUS_ALL;
    msg.rtr = true;              // RTR frame - no data
    msg.length = 0;              // No data bytes
    can_transmit(&msg);
}
```

#### 2. CAN Receive Callback
```c
void can_rx_callback(can_message_t *msg, uint8_t fmi)
{
    uint32_t current_time = GetTick();
    
    if (msg->id == FB_SLAVE1) {
        slave1_info.status = msg->data[0];
        slave1_info.running_state = msg->data[1];
        slave1_info.current_setpoint = (msg->data[2] << 8) | msg->data[3];
        slave1_info.connection_ok = true;
        slave1_info.last_response_time = current_time;
    }
    else if (msg->id == FB_SLAVE2) {
        // Similar processing for SLAVE2
    }
}
```

#### 3. Connection Timeout Check
```c
void slave_update_connection(slave_info_t *slave)
{
    uint32_t current_time = GetTick();
    
    if (slave->connection_ok) {
        if ((current_time - slave->last_response_time) > CONNECTION_TIMEOUT_MS) {
            slave->connection_ok = false;
            slave->status = 0;
        }
    }
}
```

#### 4. Timer Interrupt Handler
```c
void Timer4_IRQ_Callback(void)
{
    lcd_update_counter++;
    status_check_counter++;
    
    if (lcd_update_counter >= LCD_UPDATE_PERIOD_MS) {
        lcd_update_counter = 0;
        SR |= SR_TIMER_100MS;
        SR |= SR_LCD_UPDATE;
        SR |= SR_ADC_UPDATE;
    }
    
    if (status_check_counter >= 3000) {
        status_check_counter = 0;
        SR |= SR_STATUS_CHECK;
    }
}
```

## CAN Filter Configuration

### Bank 0: Mask Mode (0x100-0x10F)
- **Purpose**: Accept STATUS and SETPOINT_ALL commands
- **Scale**: 32-bit
- **Filter**: `FR1 = (0x100 << 21)`, `FR2 = (0x7F0 << 21)`
- **Accepted IDs**:
  - `CMD_SETPOINT_ALL = 0x100`
  - `CMD_STATUS_ALL = 0x101`
  - `CMD_STATUS_SLAVE1 = 0x102`
  - `CMD_STATUS_SLAVE2 = 0x103`

### Bank 1: List Mode (START/STOP)
- **Scale**: 16-bit
- **Commands**:
  - `CMD_START_SLAVE1 = 0x120`
  - `CMD_STOP_SLAVE1 = 0x121`
  - `CMD_START_SLAVE2 = 0x130`
  - `CMD_STOP_SLAVE2 = 0x131`

### Bank 2: List Mode (SETPOINT)
- **Scale**: 16-bit
- **Commands**:
  - `CMD_SETPOINT_SLAVE1 = 0x122`
  - `CMD_SETPOINT_SLAVE2 = 0x132`

## Operation Sequence

### Startup Sequence
1. System initialization
2. LCD displays startup message
3. Delay 100ms
4. Send initial `CMD_STATUS_ALL` RTR request
5. Enter main loop

### Main Loop (Event-Driven)
```
Every 1ms:
  - Increment lcd_update_counter
  - Increment status_check_counter

Every 100ms (SR_LCD_UPDATE):
  - Check connection timeout for both slaves
  - Update LCD display

Every 3000ms (SR_STATUS_CHECK):
  - Send CMD_STATUS_ALL RTR request
  - Reset status_check_counter

On CAN RX (can_rx_callback):
  - Parse status feedback (FB_SLAVE1/FB_SLAVE2)
  - Update slave_info structure
  - Set connection_ok = true
  - Record last_response_time
```

## LCD Display Format

```
Line 0: "RPM:      VOLT:"
Line 1: "    XXXX     XXXX"  (or "--" if disconnected)
Line 2: "SP: XXXX  SP: XX.X*"
Line 3: "UART:%XXX* ADC:%XXX*"
```

### Display Logic
- If `status == 0` OR `connection_ok == false`: Display `"--  "`
- If connected: Display actual setpoint value
- Asterisk (*) indicates active control target

## Testing Recommendations

### 1. Normal Operation Test
- Power on Master and both Slaves
- Verify initial status check sent after 100ms
- Confirm status updates every 3 seconds
- Check LCD displays actual values (not "--")

### 2. Disconnect Test
- Power off Slave1
- After 5 seconds, LCD should show "--" for Slave1
- Slave2 should continue normal operation
- Verify Slave1 line still shows "--" after 10 seconds

### 3. Reconnect Test
- Power on Slave1 again
- Within 3 seconds (next status check), Slave1 should reconnect
- LCD should display actual Slave1 value again

### 4. Startup Test
- Power on Master first (Slaves off)
- LCD should show "--" for both slaves
- Power on Slaves one by one
- Verify each reconnects within 3 seconds

## Timing Diagram

```
Time    Event
----    -----
0ms     System startup
100ms   Initial status check (CMD_STATUS_ALL RTR)
200ms   LCD update (if response received)
3000ms  Periodic status check #1
3100ms  LCD update
6000ms  Periodic status check #2
6100ms  LCD update
...     (continues every 3 seconds)

If no response for 5000ms:
        connection_ok = false
        LCD displays "--"
```

## Implementation Notes

1. **RTR Frames**: Remote frames have `rtr=true` and `length=0` with no data payload
2. **Filter Efficiency**: Bank 0 uses mask mode to accept range 0x100-0x10F efficiently
3. **Event-Driven**: Status Register (SR) flags trigger processing in main loop
4. **Non-Blocking**: All CAN operations are non-blocking to maintain real-time response
5. **Timeout Detection**: Checked before every LCD update (100ms) for quick visual feedback

## Files Modified

1. `Library/can.h` - Added CMD_STATUS_ALL and reorganized commands by filter bank
2. `Library/can.c` - Updated can_slave_setup_filters() with 3-bank configuration
3. `CAN_MASTER/main.c` - Added complete status check system:
   - slave_info_t structure enhancement
   - SR_STATUS_CHECK flag
   - status_check_counter variable
   - can_send_status_all() function
   - can_rx_callback() implementation
   - slave_update_connection() implementation
   - Timer4_IRQ_Callback() enhancement
   - lcd_update_display() disconnect detection
   - Initial status check on startup

---
**Version**: 1.0  
**Date**: 2025-01-10  
**Author**: System Engineer
