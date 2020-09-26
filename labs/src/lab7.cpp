/*
 ===============================================================================
 Name        : main.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
 ===============================================================================
 */

#if defined (__USE_LPCOPEN)
#if defined(NO_BOARD_LIB)
#include "chip.h"
#else
#include "board.h"
#endif
#endif

#include <cr_section_macros.h>

// TODO: insert other include files here

// TODO: insert other definitions and declarations here

#include "FreeRTOS.h"
#include "task.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Sets up system hardware */
static void prvSetupHardware(void) {
	SystemCoreClockUpdate();
	Board_Init();
	heap_monitor_setup();

	/* Initial LED0 state is off */
	Board_LED_Set(0, false);
}


/*****************************************************************************
 * Public functions
 ****************************************************************************/

/* the following is required if runtime statistics are to be collected */
extern "C" {

void vConfigureTimerForRunTimeStats(void) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}

}
/* end runtime statictics collection */


int main(void) {
	prvSetupHardware();

	LPC_SCT->CONFIG |= (1 << 17); // two 16-bit timers, auto limit
	LPC_SCT->CTRL_L |= (12-1) << 5; // set prescaler, SCTimer/PWM clock = 1 MHz
	LPC_SCT->MATCHREL[0].L = 10-1; // match 0 @ 10/1MHz = 10 usec (100 kHz PWM freq)
	LPC_SCT->MATCHREL[1].L = 5; // match 1 used for duty cycle (in 10 steps)
	LPC_SCT->EVENT[0].STATE = 0xFFFFFFFF; // event 0 happens in all states
	LPC_SCT->EVENT[0].CTRL = (1 << 12); // match 0 condition only
	LPC_SCT->EVENT[1].STATE = 0xFFFFFFFF; // event 1 happens in all states
	LPC_SCT->EVENT[1].CTRL = (1 << 0) | (1 << 12); // match 1 condition only
	LPC_SCT->OUT[0].SET = (1 << 0); // event 0 will set SCTx_OUT0
	LPC_SCT->OUT[0].CLR = (1 << 1); // event 1 will clear SCTx_OUT0
	LPC_SCT->CTRL_L &= ~(1 << 2); // unhalt it by clearing bit 2 of CTRL reg

	while(1){

	}

	/* Should never arrive here */
	return 1;
}

