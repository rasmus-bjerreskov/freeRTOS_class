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

#define EXER 3 //exercise selector

#include "FreeRTOS.h"
#include "task.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"

#if EXER == 1
#include <mutex>
#include "Smutex.h"
#endif

#include "semphr.h"

#if EXER == 3
#include <time.h>
#include <stdlib.h>
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
#if EXER == 3
SemaphoreHandle_t cntSemphr;
SemaphoreHandle_t mutex; //on the whole, the freertos way seems more intuitive that the c++11 way
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
static void vUARTechoTask(void *pvParameters) {
	int c;
	while (1) {
		if ((c = Board_UARTGetChar()) != EOF) {
			xSemaphoreGive(binSemphr);
			Board_UARTPutChar(c);
			Board_UARTPutSTR("\r\n");
		}
	}
}

static void vBlinkTask(void *pvParameters) {
	Board_LED_Set(1, false);
	while (1) {
		if (xSemaphoreTake(binSemphr, (TickType_t) portMAX_DELAY) == pdTRUE) {
			Board_LED_Set(1, true);
			vTaskDelay(configTICK_RATE_HZ / 10);
			Board_LED_Set(1, false);
			vTaskDelay(configTICK_RATE_HZ / 10);
		}
	}
}
#endif

#if EXER == 3
void pseudoprofound_bullshit() {
	switch (rand() % 5) {
	case 0:
		Board_UARTPutSTR(
				"Hidden meaning transforms unparalleled abstract beauty.\r\n");
		break;

	case 1:
		Board_UARTPutSTR(
				"Unparalleled transforms meaning beauty hidden abstract\r\n");
		break;

	case 2:
		Board_UARTPutSTR(
				"Attention and intention are the mechanics of manifestation.\r\n");
		break;

	case 3:
		Board_UARTPutSTR(
				"Nature is a self-regulating ecosystem of awareness\r\n");

	case 4:
		Board_UARTPutSTR(
				"We exist as a resonance cascade. Nothing is impossible. Presence requires exploration.\r\n");
		break;

	default:
		Board_UARTPutSTR("I'm a duck\r\n");
		break;
	}
}
static void vUARTechoTask(void *pvParameters) {
	int c = EOF;
	int len = 0;
	char cBuf[61];
	bool isQ = false;

	while (1) {

		while (c != '\n' && c != '\r' && len < 60) { 	//board_UARTGetChar was originally inside this condition
														//but I couldn't find a way to make it work with mutex
														//feels a bit awkward now
			if (c != EOF) {
				xSemaphoreTake(mutex, portMAX_DELAY);
				Board_UARTPutChar(c);
				xSemaphoreGive(mutex);
				cBuf[len] = c;
				len++;
				if (c == '?')
					isQ = true;
			}
			xSemaphoreTake(mutex, portMAX_DELAY);
			c = Board_UARTGetChar();
			xSemaphoreGive(mutex);
		}
		cBuf[len] = '\0';
		len = 0;
		c = EOF;	//yuck
		xSemaphoreTake(mutex, portMAX_DELAY);
		Board_UARTPutSTR("\r[You] ");
		Board_UARTPutSTR(cBuf);
		Board_UARTPutSTR("\r\n");
		xSemaphoreGive(mutex);

		if (isQ) {
			xSemaphoreGive(cntSemphr);
			//Board_UARTPutSTR("Question!\r\n");
		}
		isQ = false;
	}
}

static void vUARToracleTask(void *pvParameters) {
	while (1) {
		if (xSemaphoreTake(cntSemphr, portMAX_DELAY) == pdTRUE) {

			xSemaphoreTake(mutex, portMAX_DELAY);
			Board_UARTPutSTR("[Oracle] Hmmm...\r\n");
			xSemaphoreGive(mutex);
			vTaskDelay(configTICK_RATE_HZ * 3);

			xSemaphoreTake(mutex, portMAX_DELAY);
			Board_UARTPutSTR("[Oracle] ");
			pseudoprofound_bullshit();
			xSemaphoreGive(mutex);

			vTaskDelay(configTICK_RATE_HZ * 2);
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

	xTaskCreate(vUARTechoTask, "vUARTecho",
			configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vBlinkTask, "vTaskBlink",
			configMINIMAL_STACK_SIZE, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

#endif

#if EXER == 3
	cntSemphr = xSemaphoreCreateCounting(0xf, 0);
	mutex = xSemaphoreCreateMutex();
	srand(time(0)); //not actually sure if this is in any way meaningful in an embedded environment

	xTaskCreate(vUARTechoTask, "vUARTecho",
	configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vUARToracleTask, "vOracleTask",
	configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY) + 1UL,
			(TaskHandle_t*) NULL);
#endif
	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

