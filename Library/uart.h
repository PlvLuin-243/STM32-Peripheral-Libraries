#include "stm32f10x.h"
#include <stdint.h>

// Basic UART functions
void UART_Init(unsigned short usart,unsigned long BR);//BR:baud rate
unsigned long USART_BRR(unsigned short usart,unsigned long BR);
void USART_Init(unsigned short usart,unsigned long BR);
char UART_RX(unsigned short uart);
void UART_TX(unsigned short uart,char c);
void UART_SendData(unsigned short usart,uint8_t *pTxBuffer,uint32_t len);
void UART_ReceiveData(unsigned short usart,uint8_t *pRxBuffer,uint32_t len);

// DMA functions
void UART_DMA_TX_Init(unsigned short usart);
void UART_DMA_RX_Init(unsigned short usart, uint8_t *rx_buffer, uint16_t buffer_size);
void UART_DMA_SendData(unsigned short usart, uint8_t *data, uint16_t length);
uint8_t UART_DMA_TX_IsBusy(unsigned short usart);
void UART_DMA_Enable(unsigned short usart);

// External flags and buffers (must be defined in user code)
extern volatile uint8_t uart1_tx_complete;
extern volatile uint8_t uart2_tx_complete;
extern volatile uint8_t uart1_rx_complete;
extern volatile uint8_t uart2_rx_complete;

