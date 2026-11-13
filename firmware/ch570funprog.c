#include "ch32fun.h"
#include <stdio.h>
#include <string.h>
#include "fsusb.h"
#include "flash_storage.h"

#define PIN_SWD              PA2
#define PIN_SWC              PA3

#define PIN_SWD_MASK         (1<<2)
#define PIN_SWC_MASK         (1<<3)

#define LED                  PA7
#define LED_ON_POLARITY      0
#define PIN_TARGETPOWER      PA7

#define POWER_ON_POLARITY    0
#define POWER_OFF_POLARITY   1
#define PIN_TARGETPOWER_MODE GPIO_CFGLR_OUT_10Mhz_PP

#define USB_SERIAL_DEBUG     1

// Allow reading and writing to the scratchpad via HID control messages.
// Can we make them smaller? We have maximum allowed size in the descriptor of 264 bytes
// We are running low on RAM, so come back here when it becomes critical
__attribute__ ((aligned(4))) uint8_t scratch[511];
__attribute__ ((aligned(4))) uint8_t retbuff[511];

volatile uint32_t usb_pending = 0;
volatile uint32_t scratch_return = 0;
volatile uint32_t millis_cnt = 0;

int doreboot = 0;
uint32_t rebootat = 0;
uint8_t tty_opened = 0;
volatile uint8_t uart_enabled = 0;

int didnak;
int missed, hit;

void enable_uart(uint8_t on);

#include "funprog_config.h"
#include "funprog_port.h"
#include "../ch32fun/minichlink/funprog/funprog_rvswdio.h"
#include "bitbang_rvswdio_ch57x.h"
#include "../ch32fun/minichlink/funprog/funprog_ch5xx.h"
#include "../ch32fun/minichlink/funprog/funprog_logic.h"

#if defined(HW_VERSION) && HW_VERSION==1
#define UART_TX_PIN          PA3
#define UART_RX_PIN          PA2
#else
#define UART_TX_PIN          PA11
#define UART_RX_PIN          PA10
#endif
// #define UART_DTR_PIN
#include "uart_cdc.h"

#if (defined(USB_SERIAL_DEBUG) && USB_SERIAL_DEBUG) || FUNCONF_USE_USBPRINTF
int _write(int fd, const char *buf, int size)
{
	// Don't block if we have nowhere to output data
  if( !tty_opened || uart_enabled ) return size;
	// while( USBFSCTX.endpoints[3].busy || ((USBFS->INT_ST & 0x80)) || USBFSCTX.USBFS_errata_dont_send_endpoint_in_window || (USBFSCTX.USBFS_SetupReqLen > 0)) Delay_Ms(1);
  uint32_t send_retries = 100;
	while( USBFSCTX.endpoints[3].busy && send_retries-- ) Delay_Us(100);

	if( size )
	{
		UEP_DMA( 3 ) = (uintptr_t)USBFSCTX.ep_buffers[3];
		memcpy( USBFSCTX.endpoints[3].in, buf, size );
		UEP_CTRL_LEN( 3 ) = size;
		UEP_CTRL_TX( 3 ) = ( UEP_CTRL_TX( 3 ) & ~USBFS_UEP_T_RES_MASK ) | USBFS_UEP_T_RES_ACK;
		USBFSCTX.endpoints[3].busy = 1;
	}

	return size;
}
#if defined(FUNCONF_USE_USBPRINTF) && FUNCONF_USE_USBPRINTF
// Also need to have "poll_input" in the loop
void HandleUSBInput(int numbytes, uint8_t *data) {
	// Process data coming form the terminal
	_write(0, (const char*)data, numbytes); // loopback
}
#endif
#endif

int HandleInRequest( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	int ret = 0;  // Just NAK
	switch ( endp )
	{
		case 1:
			// ret = -1; // Just ACK
			break;
		case 3:
			cdc.rxing = 0;
			// ret = -1; // ACK, without it RX was stuck in some cases, leaving for now as a reminder
			break;
	}
	return ret;
}

void HandleDataOut( struct _USBState *ctx, int endp, uint8_t *data, int len )
{
	if( endp == 0 )
	{
		ctx->USBFS_SetupReqLen = 0; // To ACK
		if( ctx->USBFS_SetupReqCode == CDC_SET_LINE_CODING )
		{
			// Need to save incoming config, we will send it back later when asked
			cdc.cdc_cfg[0] = CTRL0BUFF[0];
			cdc.cdc_cfg[1] = CTRL0BUFF[1];
			cdc.cdc_cfg[2] = CTRL0BUFF[2];
			cdc.cdc_cfg[3] = CTRL0BUFF[3];
			cdc.cdc_cfg[4] = CTRL0BUFF[4];
			cdc.cdc_cfg[5] = CTRL0BUFF[5];
			cdc.cdc_cfg[6] = CTRL0BUFF[6];
			// First 4 bytes of config make a baudrate
			uint32_t baud = CTRL0BUFF[0];
			baud += ((uint32_t)CTRL0BUFF[1] << 8);
			baud += ((uint32_t)CTRL0BUFF[2] << 16);
			baud += ((uint32_t)CTRL0BUFF[3] << 24);
			cdc.uart->baud = baud;

			// Then one byte for stop bits (0 - 1 stop bit, 1 - 1.5 stop bit, 2 - 2 stop bits)
			// CH5xx are missing 1.5 stop bit option
			switch( CTRL0BUFF[4] )
			{
				case 0:
					cdc.uart->stop_bits = USART_StopBits_1;
					break;
				case 2:
					cdc.uart->stop_bits = USART_StopBits_2;
					break;
				default:
					USBFS_SendNAK( 0, 0 );
					break;
			}

			// Then goes one byte for parity setting (0 - No, 1- Odd, 2 - Even, 3 - Mark, 4 - Space)
			switch( CTRL0BUFF[5] )
			{
				case 0:
					cdc.uart->parity = USART_Parity_No;
					break;
				case 1:
					cdc.uart->parity = USART_Parity_Odd;
					break;
				case 2:
					cdc.uart->parity = USART_Parity_Even;
					break;
				case 3:
					cdc.uart->parity = USART_Parity_Mark;
					break;
				case 4:
					cdc.uart->parity = USART_Parity_Space;
					break;
				default:
					USBFS_SendNAK( 0, 0 );
					break;
			}

			// The last byte is word length config. It's an integer (5, 6, 7, 8, 16)
			switch( CTRL0BUFF[6] )
			{
				case 5:
					cdc.uart->word_length = USART_WordLength_5b;
					break;
				case 6:
					cdc.uart->word_length = USART_WordLength_6b;
					break;
				case 7:
					cdc.uart->word_length = USART_WordLength_7b;
					break;
				case 8:
					cdc.uart->word_length = USART_WordLength_8b;
					break;
				default:
					USBFS_SendNAK( 0, 0 );
					break;
			}
			if( uart_enabled )
			{
				uart_reset();
				uart_config( cdc.uart );
			}
		}
	}

	if( endp == 2 )
	{
		if( uart_enabled )
		{
			if( cdc.tx_stop )
			{
				cdc.tx_stop = 0;
				return;
			}

			int pad = 4 - USBFS->RX_LEN;
			if( pad < 0 ) pad = 0;
			uint32_t write_pos = cdc.tx_pos + cdc.tx_remain + USBFS->RX_LEN;
			if( write_pos > cdc.tx_wrap_pos ) write_pos -= cdc.tx_wrap_pos;

			if( write_pos & 0x3 )
			{
				write_pos = (write_pos + 4) & ~0x3;
				USBFS_SendNAK( 2, 0 );
				cdc.tx_stop = 1;
			}

			if( write_pos > ( UART_TX_BUF_SIZE - 64) )
			{
				cdc.tx_wrap_pos = cdc.tx_pos + cdc.tx_remain + USBFS->RX_LEN;
				write_pos = 0;
			}

			USBFS->UEP2_DMA = (uintptr_t)(uart_tx_buffer + write_pos);

			if( cdc.tx_remain >= ( UART_TX_BUF_SIZE ) )
			{
				USBFS_SendNAK( 2, 0 );
				cdc.tx_stop = 1;
			}
			cdc.tx_remain += USBFS->RX_LEN;
		}
	}
}

int HandleSetupCustom( struct _USBState *ctx, int setup_code )
{
	int ret = -1;
	if ( ctx->USBFS_SetupReqType & USB_REQ_TYP_CLASS )
	{
		switch ( setup_code )
		{
			case CDC_SET_LINE_CTLSTE:
			{
				ret = ( ctx->USBFS_SetupReqLen ) ? ctx->USBFS_SetupReqLen : -1;
				tty_opened = CTRL0BUFF[2] & 3;
#if defined(UART_DTR_PIN) && UART_DTR_PIN
				if( uart_eanbled )
				{
					funDigitalWrite(UART_DTR_PIN, !tty_opened);
				}
#endif
				break;
			}
			case CDC_SET_LINE_CODING:
			case CDC_SEND_BREAK:
				ret = ( ctx->USBFS_SetupReqLen ) ? ctx->USBFS_SetupReqLen : -1;
				break;
			case CDC_GET_LINE_CODING:
				ctx->pCtrlPayloadPtr = cdc.cdc_cfg;
				ret = ctx->USBFS_SetupReqLen;
				break;
			default:
				ret = 0;
				break;
		}
	}
	else if( ctx->USBFS_SetupReqType & USB_REQ_TYP_VENDOR )
	{
		// Vendor request
	}
	else
	{
		ret = 0; // Go to STALL
	}
	return ret;
}

int HandleHidUserSetReportSetup( struct _USBState * ctx, tusb_control_request_t * req )
{
	int id = req->wValue & 0xff;
	if( ( id >= 0xaa && id <= 0xae ) && req->wLength <= sizeof(scratch) )
	{
		if( usb_pending ) return 0;
		ctx->pCtrlPayloadPtr = scratch;
		usb_pending = req->wLength;
		return req->wLength;
	}
	return 0;
}

int HandleHidUserGetReportSetup( struct _USBState * ctx, tusb_control_request_t * req )
{
	int id = req->wValue & 0xff;
	switch( id )
	{
	case 0xaa:
	case 0xab:
	case 0xac:
	case 0xad:
	case 0xae:
	{
		if( scratch_return )
		{
			hit++;
			scratch_return = 0;
			ctx->pCtrlPayloadPtr = retbuff;
			if( sizeof(retbuff) - 1 < req->wLength )
				return sizeof(retbuff) - 1;
			else
				return req->wLength;
		}
		else
		{
			missed++;
			didnak = req->wLength;
			return 0;
		}
	}
	case 0xe2: // Copy the printf debug buffer out of DMDATA0.
		memcpy( scratch, (char*)DMDATA0, 8 );
		ctx->pCtrlPayloadPtr = scratch;
		*DMDATA0 = 0;
		return 8;
	}
	return -1;
}

void HandleHidUserReportDataOut( struct _USBState * ctx, uint8_t * data, int len )
{
	switch( data[0] )
	{
	case 0xe1:
		if( len > 7 )
		{
			if( memcmp( (char*)(data+1), "\xbe\xef\x00\xc0\x01\xd0\x0d", 7 ) == 0 )
			{
				doreboot = 2;
				rebootat = SysTick->CNTL + 1000000;
			}
		}
		break;
	}
}

int HandleHidUserReportDataIn( struct _USBState * ctx, uint8_t * data, int len )
{
	memset( data, 0, len );
	return len;
}

void HandleHidUserReportOutComplete( struct _USBState * ctx )
{
	return;
}

__USBFS_FUN_ATTRIBUTE
void process_cdc()
{
	uart_process_tx( &cdc );
	uart_process_rx( &cdc );
}

// Can work without this attribute (without being in RAM)
// This frees ~4K of RAM, but maybe makes process slower(?)
// Needs testing, currently leaving it like this. Preliminary testing shows negligible difference
// 240K flashed to CH570 is 14.6s vs 16.6s
__USBFS_FUN_ATTRIBUTE
static __attribute__((noreturn)) void processLoop()
{
	while(1)
	{
		//printf( "%lu %08lx %lu %d %d\n", USBDEBUG0, USBDEBUG1, USBDEBUG2, 0, 0 );
		if( doreboot )
		{
			if( (int)(SysTick->CNTL - rebootat) > 0 )
			{
				USBFSReset();
				jump_isprom();
			}
		}

/*
		struct SWIOState st;
		int r;
		r = InitializeSWDSWIO( &st );
		if( r == 0 )
		{
			uint8_t reply[8];
			r = DetermineChipTypeAndSectorInfo( &st, reply );
			printf( "Chip type: %d %02x %02x %02x %02x %02x %02x %02x %02x\n",
				r, reply[0], reply[1], reply[2], reply[3], reply[4], reply[5], reply[6], reply[7] );
			uint32_t esig = 0;
			r = ReadWord( &st, 0x1FFFF7E0, &esig );
			printf( "ESIG: %d %08x\n", r, (int)esig );
		}
		Delay_Ms(100);
*/


		if( usb_pending && !USBFSCTX.pCtrlPayloadPtr )
		{
			scratch_return = 0;
			//printf( "+%02x %02x %02x %02x %02x\n", scratch[0], scratch[1], scratch[2], scratch[3], scratch[4] );
			HandleCommandBuffer( scratch, sizeof(scratch)-1, retbuff, sizeof(retbuff)-1 );
			//printf( "-%02x %02x %02x %02x %02x\n", scratch[0], scratch[1], scratch[2], scratch[3], scratch[4] );

			__disable_irq();
			if( didnak )
			{
				struct _USBState * ctx = &USBFSCTX;

				int len = didnak;

				if( sizeof(retbuff) - 1 < len )
					len = sizeof(retbuff) - 1;

				ctx->pCtrlPayloadPtr = retbuff;

				ctx->USBFS_SetupReqLen = len;
				len = len >= DEF_USBD_UEP0_SIZE ? DEF_USBD_UEP0_SIZE : len;

				uint8_t * ctrl0buff = CTRL0BUFF;
				memcpy( ctrl0buff, ctx->pCtrlPayloadPtr, len );
				ctx->pCtrlPayloadPtr += len;

				UEP_CTRL_LEN(0) = len;
				UEP_CTRL_TX(0) = CHECK_USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_ACK | USBFS_UEP_T_TOG;
				ctx->USBFS_SetupReqLen -= len;

				didnak = 0;
			}
			__enable_irq();
			usb_pending = 0;
			scratch_return = 1;
		}

		// To service interrupt endpoint
		uint32_t * buffer = (uint32_t*)USBFS_GetEPBufferIfAvailable( USB_EP_TX );
		if( buffer )
		{
			buffer[0] = 0;
			buffer[1] = 0;
			USBFS_SendEndpoint( USB_EP_TX, 8 /* data length */ );
		}

		if( uart_enabled && tty_opened ) process_cdc();
	}
}


void setup_uart_gpio()
{
	if( !uart_enabled ) return;
	funDigitalWrite( UART_TX_PIN, 0 );
	funPinMode( UART_RX_PIN, GPIO_ModeIN_PU );
	funPinMode( UART_TX_PIN, GPIO_ModeOut_PP_5mA );
#if defined(UART_DTR_PIN) && UART_DTR_PIN
	funDigitalWrite( UART_DTR_PIN, 0 );
	funPinMode( UART_DTR_PIN, GPIO_CFGLR_OUT_2Mhz_PP );
#endif
}

void setup_swdio_gpio()
{
	funDigitalWrite( PIN_SWD, 1 );
	funDigitalWrite( PIN_SWC, 1 );
	funPinMode( PIN_SWD, GPIO_CFGLR_OUT_10Mhz_PP );
	funPinMode( PIN_SWC, GPIO_CFGLR_OUT_10Mhz_PP );
}

void enable_uart( uint8_t on )
{
	if( !on && uart_enabled )
	{
		uart_enabled = 0;
		uart_disable( &uart );
#if defined(HW_VERSION) && HW_VERSION==1
		setup_swdio_gpio();
#endif
		// printf("disabling UART\n");
	}
	else if( on && !uart_enabled )
	{
		uart_enabled = 1;
		setup_uart_gpio();
		uart_reset();
		cdc_init( &cdc );
		// printf("enabling UART\n");
	}
}

void systick_init()
{
	SysTick->CTLR = 0;
	SysTick->CMP = DELAY_MS_TIME - 1;
	SysTick->CNT = 0;
	SysTick->CTLR |= SYSTICK_CTLR_STE |  // Enable Counter
                  SYSTICK_CTLR_STIE |  // Enable Interrupts
                  SYSTICK_CTLR_STCLK;  // Set Clock Source to HCLK/1
	cdc.rx_timeout = 0;
	cdc.usb_timeout = 0;
	millis_cnt = 0;

	NVIC_EnableIRQ(SysTick_IRQn);
}

void SysTick_Handler(void) __attribute__((interrupt));
void SysTick_Handler(void)
{
	SysTick->CMP = SysTick->CNT + DELAY_MS_TIME;
	SysTick->SR = 0;
	cdc.rx_timeout++;
	cdc.usb_timeout++;
	millis_cnt++;
}

int main()
{
	SystemInit();
	systick_init();

	Delay_Ms(10);

	setup_swdio_gpio();

	uint32_t stored = read_storage();
	if( stored == MAGIC )
	{
		// This can only mean that storage wasn't initialized
		// Store first 0 word to fix this
		uart_enabled = 0;
		update_storage( 0 );
	}
	else
	{
		uart_enabled = stored&0xff;
	}

	if( uart_enabled )
	{
		setup_uart_gpio();
		cdc_init( &cdc );
	}
	// Force reset.
	funDigitalWrite( LED, !LED_ON_POLARITY );
	funPinMode( LED, GPIO_CFGLR_OUT_10Mhz_PP );

	USBFSSetup();

	funDigitalWrite( LED, LED_ON_POLARITY );

	processLoop();
}

