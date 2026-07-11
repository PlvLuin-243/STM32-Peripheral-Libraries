# STM32 CAN Master System - Sequence Diagrams

This directory contains PlantUML sequence diagrams for the 9 main user functionality sections of the STM32 CAN Master system.

## Diagram Overview

| Diagram | File | Description |
|---------|------|-------------|
| 1. System Initialization | `01_system_initialization.plantuml` | Hardware setup, peripheral configuration, and system startup |
| 2. Control Mode Selection | `02_control_mode_selection.plantuml` | Switching between UART and ADC control modes |
| 3. Target Device Selection | `03_target_device_selection.plantuml` | Selecting which slave devices to control (Slave1/Slave2/All) |
| 4. Device Start/Stop Control | `04_device_start_stop_control.plantuml` | Starting and stopping individual slave devices |
| 5. UART Control | `05_uart_control.plantuml` | Processing "Duty:XX" commands via UART with DMA |
| 6. ADC Control | `06_adc_control.plantuml` | Real-time potentiometer input processing |
| 7. Status Monitoring | `07_status_monitoring.plantuml` | Real-time monitoring of slave device status and feedback |
| 8. Continuous Setpoint Adjustment | `08_continuous_setpoint_adjustment.plantuml` | 100ms periodic setpoint updates to running slaves |
| 9. Connection Checking | `09_connection_checking.plantuml` | Heartbeat and health monitoring of slave connections |

## System Architecture

The STM32 CAN Master system controls two slave devices via CAN bus:
- **Slave1**: Motor Controller (RPM control, 0-6000 RPM)
- **Slave2**: Buck Converter (Voltage control, 0-9.00V)

### Control Modes
- **UART Mode**: Setpoints via "Duty:XX" protocol (0-100%)
- **ADC Mode**: Real-time potentiometer control (0-100%)

### User Interface
- 4 Physical buttons: MODE (PB10), SLAVE1 (PB0), SLAVE2 (PB1), CONTROL (PB11)
- LCD display showing real-time status and setpoints
- UART interface for remote control and status feedback

### Key Timing
- **1ms**: Timer4 interrupt for system timing
- **100ms**: LCD updates, ADC processing, setpoint transmission
- **1000ms**: Connection health checks (heartbeat)

## Color Coding Standard

All diagrams follow the project's color coding standard:
- **BINH (Pink)**: Internal system components and modules
- **COTS/External (White)**: Third-party hardware and external systems

## Usage

These diagrams can be rendered using PlantUML tools or online renderers. They provide detailed insight into:
- Message flow between components
- Timing relationships and sequences
- Error handling and edge cases
- Hardware interaction patterns
- Real-time system behavior

## Related Documentation

- Main code: `../CAN_MASTER/main.c`
- Project overview: `../README.md`
- Implementation guide: `Status_Check_Implementation_Summary.md`