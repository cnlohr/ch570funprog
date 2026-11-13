#ifndef _UART_CDC_H
#define _UART_CDC_H

#include <stdint.h>
#include "ch32fun.h"

#define USART_WordLength_5b 0x00
#define USART_WordLength_6b 0x01
#define USART_WordLength_7b 0x02
#define USART_WordLength_8b 0x03
#define USART_StopBits_1    0x00
#define USART_StopBits_2    0x04
#define USART_Parity_No     0x00
#define USART_Parity_Odd    0x08
#define USART_Parity_Even   0x18
#define USART_Parity_Mark   0x28
#define USART_Parity_Space  0x38

#define UART_BASE_ADDR(n)   ((uint32_t)(0x40003000+(0x400*n)))
#undef UART
#define UART(n)             ((UART_TypeDef *)UART_BASE_ADDR(n))

#define UART_TX_BUF_SIZE    512
#define UART_RX_BUF_SIZE    512
#define UART_USB_TIMEOUT    600
#define UART_ZERO_TIMEOUT   60
#define UART_RX_TIMEOUT     3

#define UART_NUMBER         1

#define UART_DEFALT_BAUD    115200
#define UART_DEFAULT_WORDL  USART_WordLength_8b
#define UART_DEFAULT_STOPB  USART_StopBits_1
#define UART_DEFAULT_PARITY USART_Parity_No

extern uint8_t uart_tx_buffer[UART_TX_BUF_SIZE];
extern uint8_t uart_rx_buffer[UART_RX_BUF_SIZE];

typedef struct {
	uint8_t number;
	uint32_t baud;
	uint16_t word_length;
	uint16_t stop_bits;
	uint16_t parity;
	uint16_t flow_control;
} UART_config_t;

typedef struct {
	UART_config_t* uart;
	uint8_t cdc_cfg[8];
	uint32_t usb_timeout;

	volatile uint8_t rxing;
	uint32_t rx_pos; // Current position in Rx buffer
	volatile uint32_t rx_remain; // Number of bytes left to send from Rx buffer to USB
	uint32_t rx_dma_cnt; // Last value of CNTR of RX buffer DMA
	uint8_t zero_packet_pending;
	uint32_t rx_timeout;
	
	uint32_t txing; // Number of bytes currently in transmission ot UART
	int tx_pos; // Outgoing position in a TX bufer
	volatile uint32_t tx_remain; // Number of unsent bytes
	uint32_t tx_wrap_pos; // Position in the buffer to wrap to 0
	uint8_t tx_stop; // Stop incomming data and let UART sent the buffer

	uint8_t error;
	
} CDC_config_t;

extern UART_config_t uart;
extern CDC_config_t cdc;

void uart_reset();
void uart_disable(UART_config_t * uart);
void uart_config(UART_config_t * uart);
void cdc_init(CDC_config_t * ctx);
void uart_process_rx(CDC_config_t * ctx);
void uart_process_tx(CDC_config_t * ctx);

#endif
