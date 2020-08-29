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

#include "FreeRTOS.h"
#include "task.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"

#if EXER == 1
#include <mutex>
#include "Smutex.h"
#endif

#if EXER == 2
#include "semphr.h"
#endif
/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/
#if EXER == 1
Smutex guard;
#endif
#if EXER == 2
SemaphoreHandle_t binSemphr;
#endif
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

# if EXER == 1

static void vSW1Task(void *pvParameters) {
	DigitalIoPin SW1(0, 17, DigitalIoPin::pullup, true);
	while (1) {

		if (SW1.read()) {
			std::lock_guard<Smutex> lock(guard);
			Board_UARTPutSTR("SW1 pressed\r\n");
		}
	}
}

static void vSW2Task(void *pvParameters) {
	DigitalIoPin SW2(1, 11, DigitalIoPin::pullup, true);
	while (1) {

		if (SW2.read()) {
			std::lock_guard<Smutex> lock(guard);
			Board_UARTPutSTR("SW2 pressed\r\n");
		}
	}
}

static void vSW3Task(void *pvParameters) {
	DigitalIoPin SW3(1, 9, DigitalIoPin::pullup, true);
	while (1) {

		if (SW3.read()) {
			std::lock_guard<Smutex> lock(guard);
			Board_UARTPutSTR("SW3 pressed\r\n");
		}
	}
}

#endif
#if EXER == 2
static void vUARTecho(void *pvParameters) {
	int c;
	while (1) {
		xSemaphoreTake(binSemphr, (TickType_t ) 1);
		if ((c = Board_UARTGetChar()) != EOF) {
			Board_UARTPutChar(c);
			Board_UARTPutSTR("\r\n");
			xSemaphoreGive(binSemphr);
		}
	}
}

static void vBlinkTask(void *pvParameters) {
	Board_LED_Set(1, 0);
	while (1) {
		if (xSemaphoreTake(binSemphr, portMAX_DELAY) == pdTRUE) {
			Board_LED_Toggle(1);
			vTaskDelay(configTICK_RATE_HZ / 10);
			Board_LED_Toggle(1);
			vTaskDelay(configTICK_RATE_HZ / 10);
			xSemaphoreGive(binSemphr);
		}
	}
}
#endif

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

/**
 * @brief	main routine for FreeRTOS blinky example
 * @return	Nothing, function should not exit
 */
int main(void) {
	prvSetupHardware();

#if EXER == 1

	xTaskCreate(vSW1Task, "vTaskSW1",
	configMINIMAL_STACK_SIZE + 300, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vSW2Task, "vTaskSW2",
	configMINIMAL_STACK_SIZE + 300, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vSW3Task, "vTaskSW3",
	configMINIMAL_STACK_SIZE + 300, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

#endif
#if EXER == 2
	binSemphr = xSemaphoreCreateBinary();

	xTaskCreate(vUARTecho, "vUARTecho",
	configMINIMAL_STACK_SIZE + 200, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vBlinkTask, "vTaskBlink",
	configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

#endif
	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

