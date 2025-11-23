#include "can.h"
#include <string.h>

// ============================================================================
// PRIVATE FUNCTION DECLARATIONS
// ============================================================================

// Initialization helpers
static void can_configure_clocks(void);
static void can_configure_gpio(void);
static bool can_enter_init_mode(void);
static void can_configure_bit_timing(void);
static void can_configure_operating_mode(void);
static void can_configure_interrupts(void);
static bool can_exit_init_mode(void);
static void can_clear_mailboxes(void);
static void can_clear_fifo1(void);
static void can_reset_buffers(void);

// Transmission helpers
static bool can_find_available_mailbox(uint32_t *mailbox_index);
static void can_clear_mailbox_flags(uint32_t mailbox_index);
static void can_configure_mailbox_id(uint32_t mailbox_index, const can_message_t *msg);
static void can_configure_mailbox_dlc(uint32_t mailbox_index, const can_message_t *msg);
static void can_load_mailbox_data(uint32_t mailbox_index, const can_message_t *msg);
static void can_request_transmission(uint32_t mailbox_index);

// Reception helpers
static void can_read_received_id(void);
static void can_read_received_dlc(void);
static void can_read_received_data(void);
static void can_release_fifo1(void);
static void can_handle_fifo_errors(void);

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================
static can_message_t rx_buffer;
static volatile bool msg_received = false;
static uint8_t fmi=0;
// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

/**
 * @brief Khoi tao CAN1 cho Standard CAN 2.0A - 500 kbps @ 72MHz (PB8=RX, PB9=TX)
 */
void can_init(void)
{
    // Buoc 1: Bat clock cho CAN1, GPIOB va AFIO
    can_configure_clocks();
    
    // Buoc 2: Cau hinh GPIO pins (PB8=RX, PB9=TX) va remap CAN1
    can_configure_gpio();
    
    // Buoc 3: Vao che do Initialization Mode
    if(!can_enter_init_mode()) {
        return; // Khong vao duoc init mode - hardware co van de
    }
    
    // Buoc 4: Cau hinh bit timing cho 500kbps @ 72MHz system clock
    can_configure_bit_timing();
    
    // Buoc 5: Cau hinh che do hoat dong Standard CAN (khong Time Triggered)
    can_configure_operating_mode();
    
    // Buoc 6: Bat FIFO1 interrupts (message pending, full, overrun)
    can_configure_interrupts();
    
    // Buoc 7: Thoat che do Initialization Mode ve Normal Mode
    if(!can_exit_init_mode()) {
        return; // Khong thoat duoc init mode - cau hinh sai
    }
    
    // Buoc 8: Clear tat ca TX mailboxes (0, 1, 2) va status flags
    can_clear_mailboxes();
    
    // Buoc 9: Clear FIFO1 va tat ca pending messages
    can_clear_fifo1();
    
    // Buoc 10: Reset internal buffers va flags
    can_reset_buffers();
}

/**
 * @brief Gui Standard CAN message
 */
bool can_transmit(can_message_t *msg)
{
    // Buoc 1: Kiem tra tham so dau vao hop le
    if(!can_is_valid_standard_id(msg->id) || msg->length > CAN_MAX_DLC) {
        return false; // ID > 0x7FF hoac DLC > 8 - khong hop le
    }
    
    uint32_t mailbox_index;
    
    // Buoc 2: Tim mailbox trong (0, 1 hoac 2)
    if(!can_find_available_mailbox(&mailbox_index)) {
        return false; // Tat ca 3 mailbox dang ban - thu lai sau
    }
    
    // Buoc 3: Clear cac flags cu cua mailbox nay
    can_clear_mailbox_flags(mailbox_index);
    
    // Buoc 4: Cau hinh Standard ID (11-bit) va RTR flag
    can_configure_mailbox_id(mailbox_index, msg);
    
    // Buoc 5: Cau hinh Data Length Code (0-8 bytes)
    can_configure_mailbox_dlc(mailbox_index, msg);
    
    // Buoc 6: Load data vao TDLR va TDHR registers (neu khong phai RTR)
    can_load_mailbox_data(mailbox_index, msg);
    
    // Buoc 7: Request transmission - set TXRQ bit
    can_request_transmission(mailbox_index);
    
    return true;
}

/**
 * @brief Interrupt handler cho Standard CAN RX FIFO1
 * @note Ham nay duoc goi tu hardware interrupt khi co message moi
 * @warning Trong interrupt context - phai nhanh va khong blocking
 */
void CAN1_RX1_IRQHandler(void)
{
    // Buoc 1: Kiem tra FIFO1 co message pending khong
    if(CAN1->RF1R & CAN_RF1R_FMP1) {
        
        // Buoc 2: Doc Standard ID (11-bit) va RTR flag tu RIR register
         fmi = (CAN1->sFIFOMailBox[1].RDTR >> 8) & 0xFF;  // FMI = bank matching filter index
			
        can_read_received_id();
        
        // Buoc 3: Doc Data Length Code (0-8) tu RDTR register
        can_read_received_dlc();
        
        // Buoc 4: Doc data bytes tu RDLR va RDHR registers
        can_read_received_data();
        
        // Buoc 5: Release FIFO1 mailbox de nhan message tiep theo
        can_release_fifo1();
        
        // Buoc 6: Set flag cho main loop biet co message moi
        msg_received = true;
        
        // Buoc 7: Goi callback function de xu ly message
        can_rx_callback(&rx_buffer, fmi);
    }
    
    // Buoc 8: Xu ly loi FIFO1 (overrun, full) neu co
    can_handle_fifo_errors();
    
    // Interrupt handler ket thuc - return ve main loop
}
/**
 * @brief Ham nhan message - BLOCKING
 */
bool can_receive(can_message_t *msg, uint32_t timeout)
{
    uint32_t start_time = 0; // Can thiem systick support
    
    while((0 - start_time) < timeout) { // Placeholder cho systick
        if(msg_received) {
            *msg = rx_buffer;
            msg_received = false;
            return true;
        }
    }
    return false;
}

/**
 * @brief Kiem tra Standard ID hop le
 */
bool can_is_valid_standard_id(uint16_t id)
{
    return (id <= CAN_STANDARD_ID_MASK);
}

/**
 * @brief Lay bus error count
 */
uint8_t can_get_bus_error_count(void)
{
    return (uint8_t)((CAN1->ESR >> 16) & 0xFF);
}

/**
 * @brief Callback yeu - nguoi dung co the override
 */
__attribute__((weak)) void can_rx_callback(can_message_t *msg, uint8_t fmi)
{
    // Nguoi dung implement trong main.c
}

// ============================================================================
// PRIVATE FUNCTIONS - INITIALIZATION
// ============================================================================

/**
 * @brief Cau hinh clocks cho CAN1, GPIOB va AFIO
 */
static void can_configure_clocks(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
}

/**
 * @brief Cau hinh GPIO pins cho CAN (PB8=RX, PB9=TX)
 */
static void can_configure_gpio(void)
{
    // Remap CAN1 tu PA11/PA12 sang PB8/PB9
    AFIO->MAPR &= ~AFIO_MAPR_CAN_REMAP;
    AFIO->MAPR |= AFIO_MAPR_CAN_REMAP_REMAP2;
    
    // PB8 = CAN_RX (Input Pull-up)
    GPIOB->CRH &= ~(GPIO_CRH_CNF8 | GPIO_CRH_MODE8);
    GPIOB->CRH |= GPIO_CRH_CNF8_1;
    GPIOB->ODR |= GPIO_ODR_ODR8;
    
    // PB9 = CAN_TX (AF Push-pull)
    GPIOB->CRH &= ~(GPIO_CRH_CNF9 | GPIO_CRH_MODE9);
    GPIOB->CRH |= GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9;
}

/**
 * @brief Vao che do Initialization
 */
static bool can_enter_init_mode(void)
{
    CAN1->MCR |= CAN_MCR_INRQ;
    
    uint32_t timeout = 10000;
    while(!(CAN1->MSR & CAN_MSR_INAK) && timeout--);
    
    return (CAN1->MSR & CAN_MSR_INAK) != 0;
}

/**
 * @brief Cau hinh bit timing @ 72MHz APB1 = 36MHz cho 500kbps
 */
static void can_configure_bit_timing(void)
{
    CAN1->BTR = 0;
    CAN1->BTR |= (8 << 0);       // BRP = 9 (8+1) - PRESCALER
    CAN1->BTR |= (2 << 16);      // TS1 = 3 (2+1) - TIME SEGMENT 1
    CAN1->BTR |= (3 << 20);      // TS2 = 4 (3+1) - TIME SEGMENT 2
    CAN1->BTR |= (0 << 24);      // SJW = 1 (0+1) - SYNC JUMP WIDTH
}

/**
 * @brief Cau hinh che do hoat dong Standard CAN
 */
static void can_configure_operating_mode(void)
{
    CAN1->MCR |= CAN_MCR_ABOM;   // Automatic bus-off management
    CAN1->MCR &= ~CAN_MCR_NART;  // Automatic retransmission enabled
    CAN1->MCR &= ~CAN_MCR_RFLM;  // FIFO not locked on overrun
    CAN1->MCR &= ~CAN_MCR_TXFP;  // Priority by identifier
    CAN1->MCR &= ~CAN_MCR_TTCM;  // Time Triggered Mode disabled
}

/**
 * @brief Cau hinh interrupts cho FIFO1
 */
static void can_configure_interrupts(void)
{
    CAN1->IER |= CAN_IER_FMPIE1;   // FIFO1 message pending interrupt
    CAN1->IER |= CAN_IER_FFIE1;    // FIFO1 full interrupt
    CAN1->IER |= CAN_IER_FOVIE1;   // FIFO1 overrun interrupt
    
    NVIC_SetPriority(CAN1_RX1_IRQn, 2);
    NVIC_EnableIRQ(CAN1_RX1_IRQn);
}

/**
 * @brief Thoat che do Initialization
 */
static bool can_exit_init_mode(void)
{
    CAN1->MCR &= ~(CAN_MCR_INRQ | CAN_MCR_SLEEP);
    
    uint32_t timeout = 10000;
    while((CAN1->MSR & CAN_MSR_INAK) && timeout--);
    
    return (CAN1->MSR & CAN_MSR_INAK) == 0;
}

/**
 * @brief Clear tat ca TX mailboxes
 */
static void can_clear_mailboxes(void)
{
    // Abort pending transmissions
    CAN1->TSR |= CAN_TSR_ABRQ0 | CAN_TSR_ABRQ1 | CAN_TSR_ABRQ2;
    
    // Clear all TX status flags
    CAN1->TSR |= CAN_TSR_RQCP0 | CAN_TSR_RQCP1 | CAN_TSR_RQCP2 |
                 CAN_TSR_TXOK0 | CAN_TSR_TXOK1 | CAN_TSR_TXOK2 |
                 CAN_TSR_ALST0 | CAN_TSR_ALST1 | CAN_TSR_ALST2 |
                 CAN_TSR_TERR0 | CAN_TSR_TERR1 | CAN_TSR_TERR2;
    
    // Clear mailbox data registers
    for(int i = 0; i < 3; i++) {
        CAN1->sTxMailBox[i].TIR  = 0x00000000;
        CAN1->sTxMailBox[i].TDTR = 0x00000000;
        CAN1->sTxMailBox[i].TDLR = 0x00000000;
        CAN1->sTxMailBox[i].TDHR = 0x00000000;
    }
}

/**
 * @brief Clear FIFO1
 */
static void can_clear_fifo1(void)
{
    // Clear pending messages
    while(CAN1->RF1R & CAN_RF1R_FMP1) {
        CAN1->RF1R |= CAN_RF1R_RFOM1;
    }
    
    // Clear error flags
    CAN1->RF1R |= CAN_RF1R_FOVR1 | CAN_RF1R_FULL1;
    
    // Clear FIFO1 mailbox data
    CAN1->sFIFOMailBox[1].RIR  = 0x00000000;
    CAN1->sFIFOMailBox[1].RDTR = 0x00000000;
    CAN1->sFIFOMailBox[1].RDLR = 0x00000000;
    CAN1->sFIFOMailBox[1].RDHR = 0x00000000;
    
    // Clear Error Status Register
    CAN1->ESR = 0;
}

/**
 * @brief Reset internal buffers
 */
static void can_reset_buffers(void)
{
    msg_received = false;
    memset(&rx_buffer, 0, sizeof(rx_buffer));
}

// ============================================================================
// PRIVATE FUNCTIONS - TRANSMISSION
// ============================================================================

/**
 * @brief Tim mailbox trong
 */
static bool can_find_available_mailbox(uint32_t *mailbox_index)
{
    uint32_t tsr = CAN1->TSR;
    
    if (!((tsr & CAN_TSR_TME0) || (tsr & CAN_TSR_TME1) || (tsr & CAN_TSR_TME2))) {
        return false;  // Tat ca mailbox dang ban
    }
    
    *mailbox_index = (tsr >> 24) & 0x03;
    return true;
}

/**
 * @brief Clear flags cua mailbox
 */
static void can_clear_mailbox_flags(uint32_t mailbox_index)
{
    uint32_t clear_flags = (CAN_TSR_RQCP0 | CAN_TSR_TXOK0 | CAN_TSR_ALST0 | CAN_TSR_TERR0) << (mailbox_index * 8);
    CAN1->TSR |= clear_flags;
}

/**
 * @brief Cau hinh ID cho mailbox
 */
static void can_configure_mailbox_id(uint32_t mailbox_index, const can_message_t *msg)
{
    CAN1->sTxMailBox[mailbox_index].TIR = 
        ((uint32_t)(msg->id & CAN_STANDARD_ID_MASK) << 21) | 
        (msg->rtr ? CAN_TI0R_RTR : 0);
}

/**
 * @brief Cau hinh DLC cho mailbox
 */
static void can_configure_mailbox_dlc(uint32_t mailbox_index, const can_message_t *msg)
{
    CAN1->sTxMailBox[mailbox_index].TDTR = (msg->length & 0x0F);
}

/**
 * @brief Load data vao mailbox
 */
static void can_load_mailbox_data(uint32_t mailbox_index, const can_message_t *msg)
{
    if(!msg->rtr && msg->length > 0) {
        // Clear data registers
        CAN1->sTxMailBox[mailbox_index].TDLR = 0;
        CAN1->sTxMailBox[mailbox_index].TDHR = 0;
        
        // Load data theo DLC
        for(uint8_t i = 0; i < msg->length && i < CAN_STANDARD_DATA_LENGTH; i++) {
            if(i < 4) {
                CAN1->sTxMailBox[mailbox_index].TDLR |=
                    ((uint32_t)msg->data[i] << (i * 8));
            } else {
                CAN1->sTxMailBox[mailbox_index].TDHR |=
                    ((uint32_t)msg->data[i] << ((i - 4) * 8));
            }
        }
    }
}

/**
 * @brief Request transmission cho mailbox
 */
static void can_request_transmission(uint32_t mailbox_index)
{
    CAN1->sTxMailBox[mailbox_index].TIR |= CAN_TI0R_TXRQ;
}

// ============================================================================
// PRIVATE FUNCTIONS - RECEPTION
// ============================================================================

/**
 * @brief Doc ID tu FIFO1
 */
static void can_read_received_id(void)
{
    rx_buffer.id = (uint16_t)((CAN1->sFIFOMailBox[1].RIR >> 21) & CAN_STANDARD_ID_MASK);
    rx_buffer.rtr = (CAN1->sFIFOMailBox[1].RIR & CAN_RI0R_RTR) ? true : false;
}

/**
 * @brief Doc DLC tu FIFO1
 */
static void can_read_received_dlc(void)
{
    rx_buffer.length = CAN1->sFIFOMailBox[1].RDTR & 0x0F;
    
    if(rx_buffer.length > CAN_MAX_DLC) {
        rx_buffer.length = CAN_MAX_DLC;
    }
}

/**
 * @brief Doc data tu FIFO1
 */
static void can_read_received_data(void)
{
    // Clear buffer truoc
    memset(rx_buffer.data, 0, CAN_STANDARD_DATA_LENGTH);
    
    // Chi doc data neu khong phai RTR frame
    if(!rx_buffer.rtr && rx_buffer.length > 0) {
        uint32_t rdlr = CAN1->sFIFOMailBox[1].RDLR;
        uint32_t rdhr = CAN1->sFIFOMailBox[1].RDHR;
        
        for(uint8_t i = 0; i < rx_buffer.length; i++) {
            if(i < 4) {
                rx_buffer.data[i] = (rdlr >> (i * 8)) & 0xFF;
            } else {
                rx_buffer.data[i] = (rdhr >> ((i - 4) * 8)) & 0xFF;
            }
        }
    }
}

/**
 * @brief Release FIFO1
 */
static void can_release_fifo1(void)
{
    CAN1->RF1R |= CAN_RF1R_RFOM1;
}

/**
 * @brief Xu ly loi FIFO
 */
static void can_handle_fifo_errors(void)
{
    if(CAN1->RF1R & CAN_RF1R_FOVR1) {
        CAN1->RF1R |= CAN_RF1R_FOVR1;
    }
    
    if(CAN1->RF1R & CAN_RF1R_FULL1) {
        CAN1->RF1R |= CAN_RF1R_FULL1;
    }
}

/**
 * @brief Cấu hình bộ lọc CAN MASTER
 */
void can_master_setup_filters(void)
{
    CAN1->FMR |= CAN_FMR_FINIT;

    // Bank 0: Slave 1 phản hồi (ID = 0x301)
    CAN1->FS1R |= CAN_FS1R_FSC0;
    CAN1->FM1R &= ~CAN_FM1R_FBM0;
    CAN1->FFA1R |= CAN_FFA1R_FFA0;
    CAN1->sFilterRegister[0].FR1 = (0x301 << 21);
    CAN1->sFilterRegister[0].FR2 = (0x7FF << 21) | 0x07;
    CAN1->FA1R |= CAN_FA1R_FACT0;

    // Bank 1: Slave 2 phản hồi (ID = 0x302)
    CAN1->FS1R |= CAN_FS1R_FSC1;
    CAN1->FM1R &= ~CAN_FM1R_FBM1;
    CAN1->FFA1R |= CAN_FFA1R_FFA1;
    CAN1->sFilterRegister[1].FR1 = (0x302 << 21);
    CAN1->sFilterRegister[1].FR2 = (0x7FF << 21) | 0x07;
    CAN1->FA1R |= CAN_FA1R_FACT1;

    CAN1->FMR &= ~CAN_FMR_FINIT;
}

/**
 * @brief Cấu hình bộ lọc CAN SLAVE
 */
void can_slave_setup_filters(void)
{
    CAN1->FMR |= CAN_FMR_FINIT;
		CAN1->FA1R = 0;
    // Bank 0: Commands chung (0x100-0x10F)
    CAN1->FS1R |= CAN_FS1R_FSC0;        // 32-bit scale
    CAN1->FM1R &= ~CAN_FM1R_FBM0;       // Mask mode
    CAN1->FFA1R |= CAN_FFA1R_FFA0;      // Gán filter 0 vào FIFO 1
    CAN1->sFilterRegister[0].FR1 = (0x100 << 21);
    CAN1->sFilterRegister[0].FR2 = (0x7F0 << 21);  // Nhận 0x100-0x10F
    CAN1->FA1R |= CAN_FA1R_FACT0;

    // Bank 1: Setpoint commands
    CAN1->FS1R &= ~CAN_FS1R_FSC1;       // 16-bit scale
    CAN1->FM1R |= CAN_FM1R_FBM1;        // List mode
    CAN1->FFA1R |= CAN_FFA1R_FFA1;      // Gán filter 1 vào FIFO 1
    
    if(SLAVE_ID == 1) {
        // Slave 1: nhận 0x201 (ALL) và 0x211 (SLAVE1)
        CAN1->sFilterRegister[1].FR1 = (0x201 << 5) << 16 | (0x211 << 5);
    } else {
        // Slave 2: nhận 0x201 (ALL) và 0x221 (SLAVE2)
        CAN1->sFilterRegister[1].FR1 = (0x201 << 5) << 16 | (0x221 << 5);
    }
    CAN1->sFilterRegister[1].FR2 = 0;
    CAN1->FA1R |= CAN_FA1R_FACT1;
    CAN1->FMR &= ~CAN_FMR_FINIT;    // Thoat filter initialization
}