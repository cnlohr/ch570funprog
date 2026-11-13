#include "uart_cdc.h"
#include "fsusb.h"
#include "ch32fun.h"
#include <string.h>

__attribute__ ((aligned(4))) uint8_t uart_tx_buffer[UART_TX_BUF_SIZE];
__attribute__ ((aligned(4))) uint8_t uart_rx_buffer[UART_RX_BUF_SIZE];

UART_config_t uart;
CDC_config_t cdc;
// volatile uint8_t uart_debug = 0;

void uart_reset() {
	// Reset UART state
	// Manually clearing FIFO, because CH570 doesn't have reset UART function
	for(int i = 0; i <= UART(cdc.uart->number)->RFC; i++)
	{
		UART(cdc.uart->number)->THR_RBR;
	}
	memset(uart_tx_buffer, 0, UART_TX_BUF_SIZE);
	memset(uart_rx_buffer, 0, UART_RX_BUF_SIZE);
	cdc.rx_remain = 0;
	cdc.rx_pos = 0;
	cdc.rxing = 0;
	cdc.tx_pos = 0;
	cdc.tx_remain = 0;
	cdc.txing = 0;
	cdc.rx_dma_cnt = 0;
	cdc.tx_stop = 1;
  cdc.usb_timeout = 0;
  cdc.rx_timeout = 0;
	USBFS->UEP2_DMA = (uintptr_t)(uart_tx_buffer);

	// UEP_CTRL_RX(2) &= ~USBFS_UEP_R_RES_MASK;
  // UEP_CTRL_RX(2) |= USBFS_UEP_R_RES_ACK;
}

void uart_disable(UART_config_t * uart) {
  uint8_t i = uart->number;
  UART(i)->FCR &= ~RB_FCR_FIFO_EN;
  UART(i)->IER = 0;
}

void uart_config(UART_config_t * uart) {
	uint8_t i = uart->number;
	UART(i)->FCR = RB_FCR_FIFO_EN;
	UART(i)->LCR = uart->word_length | uart->parity | uart->stop_bits;
	UART(i)->DIV = 1;
	UART(i)->IER = RB_IER_TXD_EN;
	UART(i)->DL = ((10 * FUNCONF_SYSTEM_CORE_CLOCK / 8 / uart->baud) + 5) / 10 ;
	Delay_Ms(1);
}

void cdc_init(CDC_config_t * ctx) {

	uart.number = UART_NUMBER;
	uart.baud = UART_DEFALT_BAUD;
	uart.word_length = UART_DEFAULT_WORDL;
	uart.stop_bits = UART_DEFAULT_STOPB;
	uart.parity = UART_DEFAULT_PARITY;
	uart.flow_control = 0;

	cdc.uart = &uart;
	cdc.tx_wrap_pos = UART_TX_BUF_SIZE;

	cdc.cdc_cfg[0] = (uint8_t)(UART_DEFALT_BAUD);
	cdc.cdc_cfg[1] = (uint8_t)(UART_DEFALT_BAUD >> 8);
	cdc.cdc_cfg[2] = (uint8_t)(UART_DEFALT_BAUD >> 16);
	cdc.cdc_cfg[3] = (uint8_t)(UART_DEFALT_BAUD >> 24);
	cdc.cdc_cfg[4] = UART_DEFAULT_STOPB;
	cdc.cdc_cfg[5] = UART_DEFAULT_PARITY;
	cdc.cdc_cfg[6] = 8;
	cdc.cdc_cfg[7] = UART_RX_TIMEOUT * 10;

	uart_config(cdc.uart);

	USBFS->UEP2_DMA = (uintptr_t)uart_tx_buffer;
}

void uart_process_rx(CDC_config_t * ctx) {
	
	uint32_t packlen = 0;
	int ret;
	NVIC_DisableIRQ(USB_IRQn);

	if (UART(ctx->uart->number)->RFC) {
		// if (uart_debug) printf("\nuart rxing = %d, rx_remain = %ld, RFC = %d\n", ctx->rxing, ctx->rx_remain, UART(ctx->uart->number)->RFC);
		
		int i;
		uint32_t pos = ctx->rx_pos+ctx->rx_remain;
		
		for (i = 0; i <= UART(ctx->uart->number)->RFC; i++) {
			if ((pos + i) >= UART_RX_BUF_SIZE) pos -= UART_RX_BUF_SIZE;
			uart_rx_buffer[pos+i] = UART(ctx->uart->number)->THR_RBR;
		}
		
		if ((ctx->rx_remain += i) > UART_RX_BUF_SIZE) {
			// printf("[RX buffer overflow]: %ld\n", ctx->rx_remain);
			ctx->rx_remain = UART_RX_BUF_SIZE;
		}
		ctx->rx_timeout = 0;
	}

	NVIC_EnableIRQ(USB_IRQn);
	if (ctx->rx_remain) {
		if (ctx->rxing == 0) {

			if( ctx->rx_remain >= USBFS_PACKET_SIZE ) packlen = USBFS_PACKET_SIZE;
			else if (ctx->rx_timeout >= UART_RX_TIMEOUT) packlen = ctx->rx_remain;
			
			if (packlen > (UART_RX_BUF_SIZE - ctx->rx_pos)) packlen = (UART_RX_BUF_SIZE - ctx->rx_pos);

			if (packlen) {
				NVIC_DisableIRQ(USB_IRQn);
				ctx->usb_timeout = 0;
				ret = USBFS_SendEndpointNEW(3, (uint8_t*)(uart_rx_buffer + ctx->rx_pos), packlen, 1);
				if (ret == 0) {
					ctx->rxing = packlen;
					ctx->rx_remain -= packlen;
					ctx->rx_pos += packlen;
					if (ctx->rx_pos >= UART_RX_BUF_SIZE) ctx->rx_pos = 0;
					// Indicate we are done sending for now, so host can pass data further
					// https://electronics.stackexchange.com/questions/253669/usb-cdc-help-with-zero-packets
					if (packlen == USBFS_PACKET_SIZE) ctx->zero_packet_pending = 1;
				}
				NVIC_EnableIRQ(USB_IRQn);
			}
		} else {
			// Manually clear rxing flag if haven't receive IN request
			if (ctx->usb_timeout >= UART_USB_TIMEOUT) {
				ctx->rxing = 0;
				USBFSCTX.endpoints[3].busy = 0;
			}
		}
	}
	if (ctx->zero_packet_pending && ctx->rxing == 0 && ctx->usb_timeout >= UART_ZERO_TIMEOUT) {
		NVIC_DisableIRQ( USB_IRQn );
		ctx->usb_timeout = 0;
		ctx->rxing = 1;
		USBFS_SendACK(3, 1);
		ctx->zero_packet_pending = 0;
		NVIC_EnableIRQ( USB_IRQn );
	}
}

void uart_process_tx(CDC_config_t * ctx) {
	
	int local_pos = ctx->tx_pos;
	// ctx->txing = 1;
	while (ctx->tx_remain && UART(ctx->uart->number)->TFC != UART_FIFO_SIZE) {
		UART(ctx->uart->number)->THR_RBR = uart_tx_buffer[local_pos++];
		if (ctx->tx_wrap_pos <= local_pos) {
			local_pos = 0;
			ctx->tx_wrap_pos = UART_TX_BUF_SIZE;
		}
		ctx->tx_remain--;
	}

  ctx->tx_pos = local_pos;

	if (ctx->tx_stop && !ctx->tx_remain && UART(ctx->uart->number)->TFC == 0) {
		NVIC_DisableIRQ(USB_IRQn);
		if( ctx->tx_pos & 0x3 ) ctx->tx_pos = (ctx->tx_pos + 4) & ~0x3;
		ctx->txing = 0;
		USBFS_SendACK(2, 0);
		ctx->tx_stop = 0;
		NVIC_EnableIRQ(USB_IRQn);
	} 
	// ctx->txing = 0;

}
