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

#define EXER 2

#include <cstring>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"
#include "ITM_write.h"
#include "user_vcom.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

DigitalIoPin *sw1;
DigitalIoPin *sw2;
DigitalIoPin *sw3;

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

int portPinVal(int port, int pin) {
	return port * 32 + pin;
}

#if EXER == 3
void pwm_init() {
	/*Ugly copy-paste setup block. Couldn't think of a less inconvenient way to generalise this
	 *
	 *cf table 201 p 234 of data sheet
	 *cf n refers to table in data sheet*/
	Chip_SCT_Init(LPC_SCTLARGE0);
	Chip_SCT_Init(LPC_SCTLARGE1);
	LPC_SCTLARGE0->CONFIG |= SCT_CONFIG_AUTOLIMIT_L | SCT_CONFIG_AUTOLIMIT_H; // two 16-bit timers, auto limit - EVENT[0] will clear timer cf 202
	LPC_SCTLARGE1->CONFIG |= SCT_CONFIG_AUTOLIMIT_L; //two 16-bit timers, auto limt

	LPC_SCTLARGE0->CTRL_L |= ((Chip_Clock_GetSystemClockRate() / 1000000 - 1)
			<< 5); // set prescaler, SCTimer/PWM clock = 1 MHz cf 203 p 240
	LPC_SCTLARGE0->CTRL_H |= ((Chip_Clock_GetSystemClockRate() / 1000000 - 1)
			<< 5); // set prescaler, SCTimer/PWM clock = 1 MHz cf 203 p 240
	LPC_SCTLARGE1->CTRL_L |= ((Chip_Clock_GetSystemClockRate() / 1000000 - 1)
			<< 5); // set prescaler, SCTimer/PWM clock = 1 MHz cf 203 p 240

	LPC_SCTLARGE0->MATCHREL[0].L = 255 - 1; // match 0 @ 1000/1MHz = 1ms period
	LPC_SCTLARGE0->MATCHREL[1].L = 255;	// match 1 defines duty cycle (inverted)
	LPC_SCTLARGE0->MATCHREL[0].H = 255 - 1; // match 0 @ 1000/1MHz = 1ms period
	LPC_SCTLARGE0->MATCHREL[1].H = 255;	// match 1 defines duty cycle (inverted)
	LPC_SCTLARGE1->MATCHREL[0].L = 255 - 1; // match 0 @ 1000/1MHz = 1ms period
	LPC_SCTLARGE1->MATCHREL[1].L = 255;	// match 1 defines duty cycle (inverted)

	LPC_SCTLARGE0->EVENT[0].STATE = 0xFFFFFFFF; // event 0 happens in all states
	LPC_SCTLARGE0->EVENT[0].CTRL = (1 << 12); // match 0 condition only
	LPC_SCTLARGE0->EVENT[1].STATE = 0xFFFFFFFF; // event 1 happens in all states
	LPC_SCTLARGE0->EVENT[1].CTRL = (1 << 0) | (1 << 12); // match 1 condition only

	LPC_SCTLARGE0->EVENT[2].STATE = 0xFFFFFFFF; // event 2 happens in all states
	LPC_SCTLARGE0->EVENT[2].CTRL = (1 << 12) | (1 << 4); // match 0 condition only - select H match register
	LPC_SCTLARGE0->EVENT[3].STATE = 0xFFFFFFFF; // event 3 happens in all states
	LPC_SCTLARGE0->EVENT[3].CTRL = (1 << 0) | (1 << 12) | (1 << 4); // match 1 condition only - select H match register

	LPC_SCTLARGE1->EVENT[0].STATE = 0xFFFFFFFF; // event 0 happens in all states
	LPC_SCTLARGE1->EVENT[0].CTRL = (1 << 12); // match 0 condition only
	LPC_SCTLARGE1->EVENT[1].STATE = 0xFFFFFFFF; // event 1 happens in all states
	LPC_SCTLARGE1->EVENT[1].CTRL = (1 << 0) | (1 << 12); // match 1 condition only

	Chip_SWM_MovablePinAssign(SWM_SCT0_OUT0_O, portPinVal(0, 25)); //assign red led as sct0 out[0} cf 114
	Chip_SWM_MovablePinAssign(SWM_SCT0_OUT1_O, portPinVal(0, 3)); //assign green led as sct0 out[1] cf 114
	Chip_SWM_MovablePinAssign(SWM_SCT1_OUT0_O, portPinVal(1, 1)); //assign blue led as sct1 out[0] cf 114

	LPC_SCTLARGE0->OUT[0].SET = (1 << 0); // event 0 will set SCT0_OUT0 - cf 231
	LPC_SCTLARGE0->OUT[0].CLR = (1 << 1); // event 1 will clear SCT0_OUT0 - cf 230
	LPC_SCTLARGE0->OUT[1].SET = (1 << 2); // event 2 will set SCT0_OUT1 - cf 231
	LPC_SCTLARGE0->OUT[1].CLR = (1 << 3); // event 3 will clear SCT0_OUT1 - cf 230
	LPC_SCTLARGE1->OUT[0].SET = (1 << 0); // event 0 will set SCT1_OUT0 - cf 231
	LPC_SCTLARGE1->OUT[0].CLR = (1 << 1); // event 1 will clear SCT1_OUT0 - cf 230

	LPC_SCTLARGE0->CTRL_L &= ~(1 << 2); // unhalt it by clearing bit 2 of CTRL reg
	LPC_SCTLARGE0->CTRL_H &= ~(1 << 2); // unhalt it by clearing bit 2 of CTRL reg
	LPC_SCTLARGE1->CTRL_L &= ~(1 << 2); // unhalt it by clearing bit 2 of CTRL reg
}

void set_pwm(char* str){
	 int i = strtol(str, NULL, 16);

	 uint16_t r, g, b;
	 r = ((i & 0xff << 16) >> 16);  //xx 00 00 red
	 g = (i & 0xff << 8) >> 8;		//00 xx 00 green
	 b = i & 0xff;					//00 00 xx blue

	 LPC_SCTLARGE0->MATCHREL[1].L = 255 - r;
	 LPC_SCTLARGE0->MATCHREL[1].H = 255 - g;
	 LPC_SCTLARGE1->MATCHREL[1].L = 255 - b;
}

/*set color of led with terminal command and hex code*/
static void vTask1(void *pvParameters) {
	pwm_init();
	ITM_init();

	char buf[8];
	char str[40] = "";
	vTaskDelay(100);
	uint32_t len = 0;
	int strlen = 0;
	while (1) {

		len = USB_receive((uint8_t*) buf, 7);
		//I'm terrible at string handling, this is the best I could do for building a string from usb
		if (len > 0) {
			buf[len] = 0; /* make sure we have a zero at the end so that we can print the data */
			ITM_write(buf);
			USB_send((uint8_t*)buf, len);
			strlen += len;

			if (strlen < 30 && buf[len - 1] != '\n' && buf[len - 1] != '\r') {
				strncat(str, buf, len); //we want to keep previous reads
			} else {
				strlen = 0;

				if (strncmp(str, "rgb #", 5) == 0) {
					ITM_write("Success\n");
					strtok(str, "#");	//discarding token before #
					char * tok = strtok(NULL, "#");
					set_pwm(tok);
				} else {
					ITM_write("Bad input\n");

				}
				str[0] = 0; //reset string after parsed input
				USB_send((uint8_t*)"\r\n", 2);
			}

		}
	}
}

#elif EXER == 2

#define MAX_SERVO_RANGE 2000
#define MIN_SERVO_RANGE 1000
#define SERVO_MID 1500
/*move servo arm with buttons*/
static void vTask1(void *pvParameters) {
	sw1 = new DigitalIoPin(0, 8, DigitalIoPin::pullup, true);
	sw2 = new DigitalIoPin(1, 6, DigitalIoPin::pullup, true);
	sw3 = new DigitalIoPin(1, 8, DigitalIoPin::pullup, true);

	uint16_t duty = SERVO_MID;

	/* cf table 201 p 234 of data sheet
	 cf n refers to table in data sheet*/
	Chip_SCT_Init(LPC_SCTLARGE0);
	LPC_SCTLARGE0->CONFIG |= SCT_CONFIG_AUTOLIMIT_L; // two 16-bit timers, auto limit - EVENT[0] will clear timer cf 202
	LPC_SCTLARGE0->CTRL_L |= ((Chip_Clock_GetSystemClockRate() / 1000000 - 1)
			<< 5); // set prescaler, SCTimer/PWM clock = 1 MHz cf 203 p 240
	LPC_SCTLARGE0->MATCHREL[0].L = 20000 - 1; // match 0 @ 20000/1MHz = 20ms period
	LPC_SCTLARGE0->MATCHREL[1].L = duty; //non-inverted - 1000 = 1ms etc
	LPC_SCTLARGE0->EVENT[0].STATE = 0xFFFFFFFF; // event 0 happens in all states
	LPC_SCTLARGE0->EVENT[0].CTRL = (1 << 12); // match 0 condition only
	LPC_SCTLARGE0->EVENT[1].STATE = 0xFFFFFFFF; // event 1 happens in all states
	LPC_SCTLARGE0->EVENT[1].CTRL = (1 << 0) | (1 << 12); // match 1 condition only

	Chip_SWM_MovablePinAssign(SWM_SCT0_OUT0_O, portPinVal(0, 10)); //assign servo motor as output cf 114

	LPC_SCTLARGE0->OUT[0].SET = (1 << 0); // event 0 will set SCT0_OUT0 - cf 231
	LPC_SCTLARGE0->OUT[0].CLR = (1 << 1); // event 1 will clear SCT0_OUT0 - cf 230
	LPC_SCTLARGE0->CTRL_L &= ~(1 << 2); // unhalt it by clearing bit 2 of CTRL reg

	bool SW1, SW2, SW3;

	ITM_init();
	vTaskDelay(200);
	while (1) {
		SW1 = sw1->read();
		SW2 = sw2->read();
		SW3 = sw3->read();

		if (SW2) { //if multiple buttons held, resetting takes priority
			duty = SERVO_MID;
		}
		else if (SW1)
			duty--;
		else if (SW3)
			duty++;

		if (duty > MAX_SERVO_RANGE)
			duty = MAX_SERVO_RANGE;
		else if (duty < MIN_SERVO_RANGE)
			duty = MIN_SERVO_RANGE;
		LPC_SCTLARGE0->MATCHREL[1].L = duty;

		vTaskDelay(5);
	}
}

#elif EXER == 1
static void vTask1(void *pvParameters) {
sw1 = new DigitalIoPin(0, 17, DigitalIoPin::pullup, true);
sw2 = new DigitalIoPin(1, 11, DigitalIoPin::pullup, true);
sw3 = new DigitalIoPin(1, 9, DigitalIoPin::pullup, true);

/* cf table 201 p 234 of data sheet
 cf n refers to table in data sheet*/
Chip_SCT_Init(LPC_SCTLARGE0);
LPC_SCTLARGE0->CONFIG |= SCT_CONFIG_AUTOLIMIT_L; // two 16-bit timers, auto limit - EVENT[0] will clear timer cf 202
LPC_SCTLARGE0->CTRL_L |= ((Chip_Clock_GetSystemClockRate() / 1000000 - 1)
		<< 5);// set prescaler, SCTimer/PWM clock = 1 MHz cf 203 p 240
LPC_SCTLARGE0->MATCHREL[0].L = 1000 - 1;// match 0 @ 1000/1MHz = 1ms period
LPC_SCTLARGE0->MATCHREL[1].L = 950;
LPC_SCTLARGE0->EVENT[0].STATE = 0xFFFFFFFF;// event 0 happens in all states
LPC_SCTLARGE0->EVENT[0].CTRL = (1 << 12);// match 0 condition only
LPC_SCTLARGE0->EVENT[1].STATE = 0xFFFFFFFF;// event 1 happens in all states
LPC_SCTLARGE0->EVENT[1].CTRL = (1 << 0) | (1 << 12);// match 1 condition only

Chip_SWM_MovablePinAssign(SWM_SCT0_OUT0_O, portPinVal(0, 3));//assign green led as output cf 114

LPC_SCTLARGE0->OUT[0].SET = (1 << 0);// event 0 will set SCT0_OUT0 - cf 231
LPC_SCTLARGE0->OUT[0].CLR = (1 << 1);// event 1 will clear SCT0_OUT0 - cf 230
LPC_SCTLARGE0->CTRL_L &= ~(1 << 2);// unhalt it by clearing bit 2 of CTRL reg

#define L_DIF 50
#define S_DIF 2

bool SW1, SW2, SW3;
int duty = 950;
int i = 0; 	//used for red led debug
char strbuf[30];
ITM_init();
vTaskDelay(200); //giving pins time to settle
/*adjust brightness of red led with sw1 and sw2, increase speed if sw3 held*/
while (1) {
	SW1 = sw1->read();
	SW2 = sw2->read();
	SW3 = sw3->read();

	if (SW1) { //increase brightness
		if (SW3)
		duty -= L_DIF;
		else
		duty -= S_DIF;
	} else if(SW2) { //decrease brightness
		if (SW3)
		duty += L_DIF;
		else
		duty += S_DIF;
	}

	if (duty > 1000) {
		duty = 1000;
	}
	else if (duty < 0) {
		duty = 0;
		i = 5;
	}
#if 0
	if (i > 0) {		//set red if max brightness hit
		Board_LED_Set(1, true);
		i--;
	} else
	Board_LED_Set(1, false);
#endif
	LPC_SCTLARGE0->MATCHREL[1].L = duty;

	if (SW1 || SW2) {
		sprintf(strbuf, "Duty cycle: %.1f%%\n", (float)(1000-duty) / 10);
		ITM_write(strbuf);
	}
	vTaskDelay(50);
}
}
#endif

int main(void) {
	prvSetupHardware();

	xTaskCreate(vTask1, "task1", configMINIMAL_STACK_SIZE * 3, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);

#if EXER == 3
	xTaskCreate(cdc_task, "CDC",
	configMINIMAL_STACK_SIZE * 3, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);
#endif

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

