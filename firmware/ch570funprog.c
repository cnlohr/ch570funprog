#include "ch32fun.h"
#include <stdio.h>
#include <string.h>
#include "fsusb.h"

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
__attribute__ ((aligned(4))) uint8_t scratch[511];
__attribute__ ((aligned(4))) uint8_t retbuff[511];

volatile uint32_t usb_pending = 0;
volatile uint32_t scratch_return = 0;

#include "funprog_config.h"
#include "funprog_port.h"
#include "../ch32fun/minichlink/funprog/funprog_rvswdio.h"
#include "bitbang_rvswdio_ch57x.h"
#include "../ch32fun/minichlink/funprog/funprog_ch5xx.h"
#include "../ch32fun/minichlink/funprog/funprog_logic.h"

int doreboot = 0;
uint32_t rebootat = 0;
uint8_t tty_open = 0;

int didnak;

int missed, hit;

#if (defined(USB_SERIAL_DEBUG) && USB_SERIAL_DEBUG) || FUNCONF_USE_USBPRINTF
int _write(int fd, const char *buf, int size)
{
  if( !tty_open ) return size; // skip because we have nowhere to output
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
	return 0;
}

void HandleDataOut( struct _USBState *ctx, int endp, uint8_t *data, int len )
{

}

int HandleSetupCustom( struct _USBState *ctx, int setup_code )
{
	int ret = -1;
	if ( ctx->USBFS_SetupReqType & USB_REQ_TYP_CLASS )
	{
		switch ( setup_code )
		{
      case CDC_SET_LINE_CTLSTE:
        ret = ( ctx->USBFS_SetupReqLen ) ? ctx->USBFS_SetupReqLen : -1;
        tty_open = CTRL0BUFF[2] & 3;
        break;
			case CDC_SET_LINE_CODING:
			case CDC_SEND_BREAK:
        ret = ( ctx->USBFS_SetupReqLen ) ? ctx->USBFS_SetupReqLen : -1;
        break;
			case CDC_GET_LINE_CODING:
        ret = ctx->USBFS_SetupReqLen;
        break;
			default:
        ret = 0;
        break;
		}
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
static __attribute__((noreturn)) void processLoop()
{
	while(1)
	{
		//printf( "%lu %08lx %lu %d %d\n", USBDEBUG0, USBDEBUG1, USBDEBUG2, 0, 0 );
		if( doreboot )
		{
			if( (int)(SysTick->CNTL - rebootat) > 0 )
			{
#if defined(CH5xx)
				USBFSReset();
				jump_isprom();
#else
				// There aren't any other chips that can reboot into USB bootloader, are there?
#endif
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
	}
}

int main()
{
	SystemInit();

	funGpioInitAll();
	Delay_Ms(10);

	// Force reset.
	funDigitalWrite( LED, !LED_ON_POLARITY );
	funPinMode( LED, GPIO_CFGLR_OUT_10Mhz_PP );

	funDigitalWrite( PIN_SWD, 1 );
	funDigitalWrite( PIN_SWC, 1 );
	funPinMode( PIN_SWD, GPIO_CFGLR_OUT_10Mhz_PP );
	funPinMode( PIN_SWC, GPIO_CFGLR_OUT_10Mhz_PP );

	USBFSSetup();

	funDigitalWrite( LED, LED_ON_POLARITY );

	processLoop();
}

