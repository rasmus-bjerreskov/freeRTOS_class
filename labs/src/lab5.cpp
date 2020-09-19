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
#include "FreeRTOS.h"
#include "task.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"
#include "semphr.h"
#include "ITM_write.h"

// TODO: insert other definitions and declarations here

#define EXER 	1

#define RIGHT 	0
#define LEFT 	1

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

DigitalIoPin *dir;
DigitalIoPin *step;
DigitalIoPin *lim1;
DigitalIoPin *lim2;
//DigitalIoPin *lim3;
//DigitalIoPin *lim4;
DigitalIoPin *btn1;
DigitalIoPin *btn2;
DigitalIoPin *btn3;

SemaphoreHandle_t rdy;

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

void one_step() {
	step->write(true);
	vTaskDelay(1);
	step->write(false);
	vTaskDelay(1);
}

static void vTask1(void *pvParameters) {
	rdy = xSemaphoreCreateBinary();

	ITM_init();

	step = new DigitalIoPin(0, 24, DigitalIoPin::output, true);
	dir = new DigitalIoPin(1, 0, DigitalIoPin::output, true);
	lim1 = new DigitalIoPin(0, 9, DigitalIoPin::pullup, true);
	lim2 = new DigitalIoPin(0, 29, DigitalIoPin::pullup, true);
//	lim3 = new DigitalIoPin(0, 0, DigitalIoPin::pullup, true);
//	lim4 = new DigitalIoPin(1, 3, DigitalIoPin::pullup, true);
	btn1 = new DigitalIoPin(0, 8, DigitalIoPin::pullup, true);
	btn2 = new DigitalIoPin(1, 6, DigitalIoPin::pullup, true);
	btn3 = new DigitalIoPin(1, 8, DigitalIoPin::pullup, true);

	bool lLim = false;
	bool rLim = false;
	int lTimer = 0;
	int lLast = 0;
	int rTimer = 0;
	int rLast = 0;

	vTaskDelay(100);
#if EXER == 2 || EXER == 3

	while (lim1->read() || lim2->read()) {
		Board_LED_Toggle(2);
		vTaskDelay(150);
	}
	Board_LED_Set(2, false);
	xSemaphoreGive(rdy);
#endif
#if EXER == 1 || EXER == 2
	while (1) {
		lLim = lim1->read(); //this really should be done with interrupts, but I hope it's okay for this exercise
		rLim = lim2->read();

		if (lLim)
			Board_LED_Set(1, true);
		else
			Board_LED_Set(1, false);

		if (rLim)
			Board_LED_Set(0, true);
		else
			Board_LED_Set(0, false);

	}
#elif EXER == 3
	while (1) {
		lLim = lim1->read();
		rLim = lim2->read();

		if (lLim) {
			lLast = lTimer;
			Board_LED_Set(0, true);
		} else if (lTimer - lLast > 1000)
			Board_LED_Set(0, false);

		if (rLim) {
			rLast = rTimer;
			Board_LED_Set(1, true);
		} else if (rTimer - rLast > 1000)
			Board_LED_Set(1, false);

		lTimer++;
		rTimer++;
		vTaskDelay(1); 	//Again I hope this is okay for this exercise.
						//Was considering using vApplicationTickHook but I wanted to move on to lab 6
	}
#endif
}

static void vTask2(void *pvParameters) {
	bool button1 = false;
	bool button3 = false;
	bool lLim = false;
	bool rLim = false;

	vTaskDelay(200);
	ITM_write("Ready\r\n");
#if EXER == 1
	while (1) {
		button1 = btn1->read();
		button3 = btn3->read();
		lLim = lim1->read();
		rLim = lim2->read();

		if (button1 || button3) {
			if (button1)
				dir->write(LEFT);
			else if (button3)
				dir->write(RIGHT);
			if (!lLim && !rLim) {
				one_step();
			}
		}
	}


#elif EXER == 2
	xSemaphoreTake(rdy, portMAX_DELAY);
	dir->write(LEFT);

	while (1) {
		lLim = lim1->read();
		rLim = lim2->read();

		if (lLim || rLim) {	//if both switches set - wait. If either set - change direction
			if (lLim && rLim) {
				while (lLim || rLim) {
					ITM_write("Waiting for switches\n");
					vTaskDelay(5000);
					lLim = lim1->read();
					rLim = lim2->read();
				}
				ITM_write("Done waiting\n");
			} else if (lLim) {
				dir->write(RIGHT);
				ITM_write("Going right\n");
			} else if (rLim) {
				dir->write(LEFT);
				ITM_write("Going left\n");
			}
		}
		one_step();
	}

#elif EXER == 3
	int width[2];
	int steps = 0;

	xSemaphoreTake(rdy, portMAX_DELAY);
	dir->write(LEFT);
	/*Ugly copy-paste section*/
	while (!lLim) { //finding left limit switch
		one_step();
		lLim = lim1->read();
	}

	dir->write(RIGHT);
	while (lLim) { 	//moving away from switch
		one_step();
		lLim = lim1->read();
	}
	for (int i = 0; i < 2; i++) {

		while (!rLim) {	//finding distance to right
			one_step();
			rLim = lim2->read();
			steps++;
		}
		dir->write(LEFT);
		while (rLim) { //moving away, retracing steps
			one_step();
			rLim = lim2->read();
			steps--;
		}
		while (!lLim) { //finding distance to left
			one_step();
			lLim = lim1->read();
			steps++;
		}
		dir->write(RIGHT);
		while (lLim) { 	//moving away, retracing steps
			one_step();
			lLim = lim1->read();
			steps--;
		}
		width[i] = steps;
		steps = 0;
	}

	int avg_width = (float) (width[0] + width[1]) / 4;
	bool direction = false; //right
	char strbuf[80];
	sprintf(strbuf, "avg width: %d\n", avg_width);
	ITM_write(strbuf);
	steps = 0;

	Board_LED_Set(2, true);
	vTaskDelay(2000);
	Board_LED_Set(2, false);

	while (1) {
		dir->write(direction);
		while (steps < avg_width) {
			one_step();
			steps++;
		}
		sprintf(strbuf, "steps: %d\n", steps);
		ITM_write(strbuf);
		steps = 0;
		direction = !direction;
	}
#endif
}
int main(void) {
	prvSetupHardware();

	xTaskCreate(vTask1, "Task1",
	configMINIMAL_STACK_SIZE * 3, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vTask2, "Task2",
	configMINIMAL_STACK_SIZE * 2, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}
