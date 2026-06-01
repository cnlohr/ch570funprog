#include "ch32fun.h"
#include <stdio.h>
#include <string.h>
#include "fsusb.h"

#define LED PA7
#define LED_ON 0

//#define POWER_IO    PA7
//#define SWC_IO      PA3
//#define SWD_IO      PA2

#define BB_PRINTF_DEBUG(x...) printf( x )
#define IRAM_ATTR __HIGH_CODE

#include "bitbang_rvswdio.h"
#include "bitbang_rvswdio_ch57x.h"

uint32_t count;
int doreboot = 0;
uint32_t rebootat = 0;
int last = 0;

void handle_debug_input( int numbytes, uint8_t * data )
{
	last = data[0];
	count += numbytes;
}

int lrx = 0;
uint8_t scratchpad[256];

int HandleHidUserSetReportSetup( struct _USBState * ctx, tusb_control_request_t * req )
{
	int id = req->wValue & 0xff;
	if( id == 0xaa && req->wLength <= sizeof(scratchpad) )
	{
		ctx->pCtrlPayloadPtr = scratchpad;
		lrx = req->wLength;
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
	{
		ctx->pCtrlPayloadPtr = scratchpad;
		if( sizeof(scratchpad) - 1 < lrx )
			return sizeof(scratchpad) - 1;
		else
			return lrx;
	}
	case 0xe2: // Copy the printf debug buffer out of DMDATA0.
		memcpy( scratchpad, (char*)DMDATA0, 8 );
		ctx->pCtrlPayloadPtr = scratchpad;
		*DMDATA0 = 0;
		return 8;
	}
	return 0;
}

void HandleHidUserReportDataOut( struct _USBState * ctx, uint8_t * data, int len )
{
	switch( data[0] )
	{
	case 0xe1:
		if( len > 7 )
		{
			if( strncmp( (char*)(data+1), "\xbe\xef\x00\xc0\x01\xd0\x0d", 7 ) == 0 )
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
/*			r = ReadWord( &st, 0x1FFFF7E4, &esig );
			printf( "  e4: %d %08x\n", r, (int)esig );
			r = ReadWord( &st, 0x1FFFF7E8, &esig );
			printf( "UNIID1: %d %08x\n", r, (int)esig );
			r = ReadWord( &st, 0x1FFFF7EC, &esig );
			printf( "UNIID2: %d %08x\n", r, (int)esig );
			r = ReadWord( &st, 0x1FFFF7F0, &esig );
			printf( "UNIID3: %d %08x\n", r, (int)esig );
*/
		}
		Delay_Ms(100);

	}
}



int main()
{
	SystemInit();

	funGpioInitAll();

	funPinMode( LED, GPIO_CFGLR_OUT_10Mhz_PP );
	funDigitalWrite( LED, !LED_ON );

	printf("USBFS starting...");

	USBFSSetup();

	printf("ok\n");

	funDigitalWrite( LED, LED_ON );

	processLoop();
}

