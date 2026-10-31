#ifndef FUNPROG_PORT_H
#define FUNPROG_PORT_H

#if DEBUG_PRINT
#define FUN_PRINTF_DEBUG(format, args...) printf(format, ##args)
#else
#define FUN_PRINTF_DEBUG(format, args...)
#endif

#define STATUS_LED_ON
#define STATUS_LED_OFF

#define POWER_SETUP do {funPinMode( PIN_TARGETPOWER, PIN_TARGETPOWER_MODE ); \
                        funDigitalWrite( PIN_TARGETPOWER, POWER_ON_POLARITY );} while(0)

#define POWER_ON do {funPinMode( PIN_TARGETPOWER, PIN_TARGETPOWER_MODE ); \
                     funDigitalWrite( PIN_TARGETPOWER, POWER_ON_POLARITY );} while(0)

#define POWER_OFF funDigitalWrite( PIN_TARGETPOWER, POWER_OFF_POLARITY )

#define DELAY_MS(n) Delay_Ms(n)
#define DELAY_US(n) Delay_Us(n)

#define SLOW_DOWN_PROTOCOL

// Platform dependatnt attribute to put functions in RAM
#define IRAM __HIGH_CODE

void EnableMCO( int use_apll, int sdm0, int sdm1, int sdm2, int odiv )
{
  return;
}

#endif
