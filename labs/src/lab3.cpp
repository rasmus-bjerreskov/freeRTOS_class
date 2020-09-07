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
#include "queue.h"
#include "semphr.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

QueueHandle_t xIntQueue;
SemaphoreHandle_t mutex;

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

static void vTaskProduce(void *pvParameters) {
	int c = EOF;
	const int max_len = 255;
	int len = 0;
	char cBuf[max_len];

	while (1) {
		while (c != '\r' && c != '\n' && len < max_len) {
			if (c != EOF) {
				xSemaphoreTake(mutex, portMAX_DELAY);
				Board_UARTPutChar(c);
				xSemaphoreGive(mutex);
				cBuf[len] = c;
				len++;
			}
			xSemaphoreTake(mutex, portMAX_DELAY); //reverse order so \r doesn't go on the array
			c = Board_UARTGetChar();
			xSemaphoreGive(mutex);
		}
		if (xIntQueue != NULL) {
			xQueueSend(xIntQueue, &len, portMAX_DELAY);
			len = 0;
			c = EOF;
		}
	}
}

static void vTaskTrigger(void *pvParameters) {
	DigitalIoPin SW1(0, 17, DigitalIoPin::pullup, true);
	bool pressed = false;
	const int minus1 = -1;
	while(1){
		if(!pressed && SW1.read()){
			if (xIntQueue != NULL){
				xQueueSend(xIntQueue, &minus1, portMAX_DELAY);
				pressed = true;
			}
		}
		if (pressed && !SW1.read())
			pressed = false;
	}
}

static void vTaskConsume(void *pvParameters) {
	mutex = xSemaphoreCreateMutex();
	xIntQueue = xQueueCreate(5, sizeof(int));
	char str[50];
	int buffer = 0;
	int count = 0;
	while (1) {
		if (xIntQueue != NULL) {
			if (xQueueReceive(xIntQueue, &buffer, portMAX_DELAY) == pdTRUE) {

				if (buffer != -1) {
					count += buffer;
				} else {
					xSemaphoreTake(mutex, portMAX_DELAY);
					sprintf(str, "You have typed %d characters\r\n", count);
					Board_UARTPutSTR(str);
					count = 0;
				}
			}
		}
	}
}

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

	xTaskCreate(vTaskProduce, "vProducerTask",
	configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vTaskTrigger, "vTriggerTask",
	configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 1UL),
			(TaskHandle_t*) NULL);

	xTaskCreate(vTaskConsume, "vConsumerTask",
	configMINIMAL_STACK_SIZE + 128, NULL, (tskIDLE_PRIORITY + 2UL),
			(TaskHandle_t*) NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

