#include "uart.h"

// Global DMA status flags
volatile uint8_t uart1_tx_complete = 1;
volatile uint8_t uart2_tx_complete = 1;
volatile uint8_t uart1_rx_complete = 0;
volatile uint8_t uart2_rx_complete = 0;

void UART_Init(unsigned short usart,unsigned long BR)//BR:baud rate
{
  //USART1:clock speed 72MHZ(PA9:TX,PA10:RX)
    //USART2:clock speed 36MHZ(PA2:TX,PA3:RX)
    //USART3:clock speed 36MHZ(PB10:TX,PB11:RX)
    
    unsigned long BRR_Cal;
    BRR_Cal=USART_BRR	(usart,BR);
    
    //Enable the Alternative Function for PINs
    RCC->APB2ENR |=(1<<0);//bit AFIOEN set 1
    
    if(usart==1)
    {
        //B1:Enable USART1
        RCC->APB2ENR |=(1<<14);//Cap clock cho USART1
        
        //B2:Enable GPIOA clock
        RCC->APB2ENR |=(1<<2);//Enable GPIOA clock
        
        //Config PA9 (TX) as AF_PP 50MHz
        GPIOA->CRH &= ~(0xF<<4);  // Clear PA9 config bits
        GPIOA->CRH |= (0xB<<4);   // PA9: Output 50MHz, AF Push-Pull (1011)
        
        //Config PA10 (RX) as Input Floating
        GPIOA->CRH &= ~(0xF<<8);  // Clear PA10 config bits  
        GPIOA->CRH |= (0x4<<8);   // PA10: Input Floating (0100)

        //B3:Setup the baud rate:9600 bits/s
        USART1->BRR =BRR_Cal;//9600
        //B4:Define the word length
        USART1->CR1 &=~(1<<12);//bit M=0:
        //B5:Config the number of stop bits
        USART1->CR2 &=~(1<<12);// 1 stop bit
        USART1->CR2 &=~(1<<13);
        //B5:enable USART
        USART1->CR1 |=(1<<13);//bit UE=1
        //B6:Enable UART transmit
        USART1->CR1 |=(1<<3);//bit TE=1
        //B7:Enable UART receive
        USART1->CR1 |=(1<<2);//bit RE=1
  
    }else if(usart==2)
    {
        //B1:Enable USART2
        RCC->APB1ENR |=(1<<17);//Cap clock cho USART2
        
        //B2:Enable GPIOA clock
        RCC->APB2ENR |=(1<<2);//Enable GPIOA clock
        
        //Config PA2 (TX) as AF_PP 50MHz
        GPIOA->CRL &= ~(0xF<<8);  // Clear PA2 config bits
        GPIOA->CRL |= (0xB<<8);   // PA2: Output 50MHz, AF Push-Pull (1011)
        
        //Config PA3 (RX) as Input Floating
        GPIOA->CRL &= ~(0xF<<12); // Clear PA3 config bits
        GPIOA->CRL |= (0x4<<12);  // PA3: Input Floating (0100)
        
        //B3:Setup the baud rate:9600 bits/s
        USART2->BRR =BRR_Cal;//9600
        //B4:Define the word length
        USART2->CR1 &=~(1<<12);//bit M=0:
        //B5:Config the number of stop bits
        USART2->CR2 &=~(1<<12);// 1 stop bit
        USART2->CR2 &=~(1<<13);
        //B5:enable USART
        USART2->CR1 |=(1<<13);//bit UE=1
        //B6:Enable UART transmit
        USART2->CR1 |=(1<<3);//bit TE=1
        //B7:Enable UART receive
        USART2->CR1 |=(1<<2);//bit RE=1	
    }
}

unsigned long USART_BRR(unsigned short usart,unsigned long BR)
{
  unsigned long div = 36000000;//fclk
    unsigned long dec;
    unsigned long final;
    double frac = 36000000.00;//phan thap phan
    double frac2 = 1.00;
    
    if(usart == 1)
    {
    div = 72000000;
    frac = 72000000.00;
    }
    div = div / (BR*16);
    frac = 16*((frac / (BR*16))-div);
    dec = frac;
    frac2 = 100*(frac-dec);
    if(frac2>50)//lam tron phan nguyen(>0.5)
    {
        dec ++;
        if(dec == 16)
        {
            dec = 0;
            div ++;
        }
    }
    
    final = (div<<4);//do div phan nguyen la bat dau tu bit 4 
    final += dec;
    
    return final;
}

char UART_RX(unsigned short uart)//cung chinh la ham ngat voi khai bao o tren UARRT
{
  char c;
    if(uart ==1)
    {
       while((USART1->SR & (1<<5))==0) {};//cho bit RXNE=1-->Du lieu san sang duoc read
            c=USART1->DR; // Doc DR tu dong xoa co RXNE
    }else if(uart==2){
       while((USART2->SR & (1<<5))==0) {};//cho bit RXNE=1-->Du lieu san sang duoc read
            c=USART2->DR; // Doc DR tu dong xoa co RXNE
    }
    return c;
}

void UART_TX(unsigned short uart,char c)
{
    if(uart ==1)
    {
        while((USART1->SR & (1<<7))==0) {};//cho bit TXE=1-->Du lieu da duoc truyen di va thanh ghi da san sang nhan du lieu moi. 
     USART1->DR=c; // Ghi DR tu dong xoa co TXE
         while((USART1->SR & (1<<6))==0) {};//wait for bit TC=1-->qua trinh truyen hoan thanh
        
    }else if(uart==2){
       while((USART2->SR & (1<<7))==0) {};//cho bit TXE=1-->Du lieu da duoc truyen di va thanh ghi da san sang nhan du lieu moi. 
     USART2->DR=c; // Ghi DR tu dong xoa co TXE
         while((USART2->SR & (1<<6))==0) {};//wait for bit TC=1-->qua trinh truyen hoan thanh
    }
}

void UART_SendData(unsigned short usart,uint8_t *pTxBuffer,uint32_t len)//do ban chat la truyen tung byte 1 nen can vong lap for
{
   uint32_t i;
    if(usart==1)
    {
    for(i=0;i<len;i++)
    {
      while((USART1->SR &(1<<7))==0){}//cho bit TXE=1
        USART1->DR = (uint8_t)(*pTxBuffer & 0xFF); // Ghi DR tu dong xoa co TXE
      pTxBuffer++;//moi lan truyen 1 byte ki tu-->sau khi truyen dc 1 ki tu can tang con tro len 1
    }
      while((USART1->SR &(1<<6))==0){}//sau khi truyen xong cho bit TC=1-->qua trinh truyen hoan thanh
    }else if(usart==2)
    {
        for(i=0;i<len;i++)
    {
      while((USART2->SR &(1<<7))==0){}//cho bit TXE=1
        USART2->DR = (uint8_t)(*pTxBuffer & 0xFF); // Ghi DR tu dong xoa co TXE
      pTxBuffer++;//moi lan truyen 1 byte ki tu-->sau khi truyen dc 1 ki tu can tang con tro len 1
    }
      while((USART2->SR &(1<<6))==0){}//sau khi truyen xong cho bit TC=1-->qua trinh truyen hoan thanh
    }
}

void UART_ReceiveData(unsigned short usart,uint8_t *pRxBuffer,uint32_t len)//do ban chat la truyen tung byte 1 nen can vong lap for
{
   uint32_t i;
    if(usart==1)
    {
     for(i=0;i<len;i++)
      {
        while((USART1->SR &(1<<5))==0){}//cho bit RXNE=1
            *pRxBuffer = (uint8_t)(USART1->DR & (uint8_t)0xFF); // Doc DR tu dong xoa co RXNE
        pRxBuffer++;//moi lan truyen 1 byte ki tu-->sau khi truyen dc 1 ki tu can tang con tro len 1
      }
    }else if(usart ==2)
    {
      for(i=0;i<len;i++)
      {
        while((USART2->SR &(1<<5))==0){}//cho bit RXNE=1
            *pRxBuffer = (uint8_t)(USART2->DR & (uint8_t)0xFF); // Doc DR tu dong xoa co RXNE
        pRxBuffer++;//moi lan truyen 1 byte ki tu-->sau khi truyen dc 1 ki tu can tang con tro len 1
      }
   }
}

//================================================================================================//
//                              UART DMA FUNCTIONS (NEW)
//================================================================================================//

//------------------------------------------------------------------------------------------------//
//                        UART DMA TX Initialization
//------------------------------------------------------------------------------------------------//
void UART_DMA_TX_Init(unsigned short usart)
{
    // Enable DMA1 clock
    RCC->AHBENR |= (1 << 0);
    
    if(usart == 1)
    {
        // DMA1 Channel 4 for USART1 TX
        DMA1_Channel4->CCR &= ~(1 << 0);  // Disable channel
        
        // Set peripheral address
        DMA1_Channel4->CPAR = (uint32_t)(&(USART1->DR));
        
        // Data size: 8-bit for both peripheral and memory
        DMA1_Channel4->CCR &= ~((0b11 << 8) | (0b11 << 10));
        
        // Configuration
        DMA1_Channel4->CCR |=  (1 << 7);   // Memory increment enable
        DMA1_Channel4->CCR &= ~(1 << 6);   // Peripheral increment disable
        DMA1_Channel4->CCR &= ~(1 << 5);   // Circular mode disable
        DMA1_Channel4->CCR |=  (1 << 4);   // Read from memory (TX direction)
        
        // Enable Transfer Complete interrupt
        DMA1_Channel4->CCR |= (1 << 1);
        NVIC_EnableIRQ(DMA1_Channel4_IRQn);
    }
    else if(usart == 2)
    {
        // DMA1 Channel 7 for USART2 TX
        DMA1_Channel7->CCR &= ~(1 << 0);  // Disable channel
        
        // Set peripheral address
        DMA1_Channel7->CPAR = (uint32_t)(&(USART2->DR));
        
        // Data size: 8-bit for both peripheral and memory
        DMA1_Channel7->CCR &= ~((0b11 << 8) | (0b11 << 10));
        
        // Configuration
        DMA1_Channel7->CCR |=  (1 << 7);   // Memory increment enable
        DMA1_Channel7->CCR &= ~(1 << 6);   // Peripheral increment disable
        DMA1_Channel7->CCR &= ~(1 << 5);   // Circular mode disable
        DMA1_Channel7->CCR |=  (1 << 4);   // Read from memory (TX direction)
        
        // Enable Transfer Complete interrupt
        DMA1_Channel7->CCR |= (1 << 1);
        NVIC_EnableIRQ(DMA1_Channel7_IRQn);
    }
}

//------------------------------------------------------------------------------------------------//
//                        UART DMA RX Initialization
//------------------------------------------------------------------------------------------------//
void UART_DMA_RX_Init(unsigned short usart, uint8_t *rx_buffer, uint16_t buffer_size)
{
    // Enable DMA1 clock
    RCC->AHBENR |= (1 << 0);
    
    if(usart == 1)
    {
        // DMA1 Channel 5 for USART1 RX
        DMA1_Channel5->CCR &= ~(1 << 0);  // Disable channel
        
        // Set addresses
        DMA1_Channel5->CPAR = (uint32_t)(&(USART1->DR));
        DMA1_Channel5->CMAR = (uint32_t)(rx_buffer);
        DMA1_Channel5->CNDTR = buffer_size;
        
        // Data size: 8-bit for both peripheral and memory
        DMA1_Channel5->CCR &= ~((0b11 << 8) | (0b11 << 10));
        
        // Configuration
        DMA1_Channel5->CCR |=  (1 << 7);   // Memory increment ENABLE (to store bytes sequentially)
        DMA1_Channel5->CCR &= ~(1 << 6);   // Peripheral increment disable
        DMA1_Channel5->CCR |=  (1 << 5);   // Circular mode enable
        DMA1_Channel5->CCR &= ~(1 << 4);   // Read from peripheral (RX direction)
        
        // Enable Transfer Complete interrupt
        DMA1_Channel5->CCR |= (1 << 1);
        NVIC_EnableIRQ(DMA1_Channel5_IRQn);
        
        // Enable channel
        DMA1_Channel5->CCR |= (1 << 0);
    }
    else if(usart == 2)
    {
        // DMA1 Channel 6 for USART2 RX
        DMA1_Channel6->CCR &= ~(1 << 0);  // Disable channel
        
        // Set addresses
        DMA1_Channel6->CPAR = (uint32_t)(&(USART2->DR));
        DMA1_Channel6->CMAR = (uint32_t)(rx_buffer);
        DMA1_Channel6->CNDTR = buffer_size;
        
        // Data size: 8-bit for both peripheral and memory
        DMA1_Channel6->CCR &= ~((0b11 << 8) | (0b11 << 10));
        
        // Configuration
        DMA1_Channel6->CCR |=  (1 << 7);   // Memory increment ENABLE (to store bytes sequentially)
        DMA1_Channel6->CCR &= ~(1 << 6);   // Peripheral increment disable
        DMA1_Channel6->CCR |=  (1 << 5);   // Circular mode enable
        DMA1_Channel6->CCR &= ~(1 << 4);   // Read from peripheral (RX direction)
        
        // Enable Transfer Complete interrupt
        DMA1_Channel6->CCR |= (1 << 1);
        NVIC_EnableIRQ(DMA1_Channel6_IRQn);
        
        // Enable channel
        DMA1_Channel6->CCR |= (1 << 0);
    }
}

//------------------------------------------------------------------------------------------------//
//                        Send Data via UART DMA
//------------------------------------------------------------------------------------------------//
void UART_DMA_SendData(unsigned short usart, uint8_t *data, uint16_t length)
{
    if(usart == 1)
    {
        // Wait if previous transfer not complete
        while(!uart1_tx_complete);
        
        uart1_tx_complete = 0;
        
        // Disable DMA channel before reconfiguration
        DMA1_Channel4->CCR &= ~(1 << 0);
        
        // Clear all DMA flags
        DMA1->IFCR |= (0x0F << 12);  // Clear all flags for Channel 4
        
        // Set memory address and data length
        DMA1_Channel4->CMAR = (uint32_t)(data);
        DMA1_Channel4->CNDTR = length;
        
        // Enable DMA channel
        DMA1_Channel4->CCR |= (1 << 0);
    }
    else if(usart == 2)
    {
        // Wait if previous transfer not complete
        while(!uart2_tx_complete);
        
        uart2_tx_complete = 0;
        
        // Disable DMA channel before reconfiguration
        DMA1_Channel7->CCR &= ~(1 << 0);
        
        // Clear all DMA flags
        DMA1->IFCR |= (0x0F << 24);  // Clear all flags for Channel 7
        
        // Set memory address and data length
        DMA1_Channel7->CMAR = (uint32_t)(data);
        DMA1_Channel7->CNDTR = length;
        
        // Enable DMA channel
        DMA1_Channel7->CCR |= (1 << 0);
    }
}

//------------------------------------------------------------------------------------------------//
//                        Check if UART DMA TX is Busy
//------------------------------------------------------------------------------------------------//
uint8_t UART_DMA_TX_IsBusy(unsigned short usart)
{
    if(usart == 1)
        return !uart1_tx_complete;
    else if(usart == 2)
        return !uart2_tx_complete;
    
    return 0;
}

//------------------------------------------------------------------------------------------------//
//                        Enable UART DMA Mode
//------------------------------------------------------------------------------------------------//
void UART_DMA_Enable(unsigned short usart)
{
    if(usart == 1)
    {
        USART1->CR3 |= (1 << 7);  // DMAT: DMA enable transmitter
        USART1->CR3 |= (1 << 6);  // DMAR: DMA enable receiver
    }
    else if(usart == 2)
    {
        USART2->CR3 |= (1 << 7);  // DMAT: DMA enable transmitter
        USART2->CR3 |= (1 << 6);  // DMAR: DMA enable receiver
    }
}

//================================================================================================//
//                           DMA INTERRUPT HANDLERS (TO BE ADDED IN main.c)
//================================================================================================//

// User must implement these handlers in their main.c:
/*
void DMA1_Channel4_IRQHandler(void)  // USART1 TX
{
    if (DMA1->ISR & (1 << 13)) {  // TCIF4
        DMA1->IFCR |= (1 << 13);
        DMA1_Channel4->CCR &= ~(1 << 0);
        uart1_tx_complete = 1;
    }
}

void DMA1_Channel5_IRQHandler(void)  // USART1 RX
{
    if (DMA1->ISR & (1 << 17)) {  // TCIF5
        DMA1->IFCR |= (1 << 17);
        uart1_rx_complete = 1;
    }
}

void DMA1_Channel7_IRQHandler(void)  // USART2 TX
{
    if (DMA1->ISR & (1 << 25)) {  // TCIF7
        DMA1->IFCR |= (1 << 25);
        DMA1_Channel7->CCR &= ~(1 << 0);
        uart2_tx_complete = 1;
    }
}

void DMA1_Channel6_IRQHandler(void)  // USART2 RX
{
    if (DMA1->ISR & (1 << 21)) {  // TCIF6
        DMA1->IFCR |= (1 << 21);
        uart2_rx_complete = 1;
    }
}
*/