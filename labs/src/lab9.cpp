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
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

#include "heap_lock_monitor.h"
#include "ITM_write.h"
#include "LpcUart.h"
#include "DigitalIoPin.h"

#include "pin_constants.h"

#include <cstdlib>

// TODO: insert other definitions and declarations here

#define EXER 1

#define SBF 80 //switch bounce filter
#define T1_BIT 1 << 1
#define T2_BIT 1 << 2
#define T3_BIT 1 << 3
#define T4_BIT 1 << 4

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

DigitalIoPin *sw1;
DigitalIoPin *sw2;
DigitalIoPin *sw3;
LpcUart *uart;

EventGroupHandle_t eGrp;
SemaphoreHandle_t binAnySw;
SemaphoreHandle_t binSw1;
SemaphoreHandle_t binSw2;
SemaphoreHandle_t binSw3;

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

extern "C" {

void PIN_INT0_IRQHandler(void) {
	ITM_write("SW1");
	static TickType_t last = 0;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
	if (xTaskGetTickCountFromISR() - last >= SBF) { //switch bounce
		last = xTaskGetTickCountFromISR();

		BaseType_t xHigherPriorityAwoken = pdFALSE;

		xSemaphoreGiveFromISR(binAnySw, &xHigherPriorityAwoken);
		xSemaphoreGiveFromISR(binSw1, &xHigherPriorityAwoken);

		portEND_SWITCHING_ISR(xHigherPriorityAwoken);
	}
}

void PIN_INT1_IRQHandler(void) {
	ITM_write("SW2");
	static TickType_t last = 0;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

	if (xTaskGetTickCountFromISR() - last >= SBF) {
		last = xTaskGetTickCountFromISR();

		BaseType_t xHigherPriorityAwoken = pdFALSE;

		xSemaphoreGiveFromISR(binAnySw, &xHigherPriorityAwoken);
		xSemaphoreGiveFromISR(binSw2, &xHigherPriorityAwoken);

		portEND_SWITCHING_ISR(xHigherPriorityAwoken);
	}
}

void PIN_INT2_IRQHandler(void) {
	ITM_write("SW3");
	static TickType_t last = 0;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);

	if (xTaskGetTickCountFromISR() - last >= SBF) {
		last = xTaskGetTickCountFromISR();

		BaseType_t xHigherPriorityAwoken = pdFALSE;

		xSemaphoreGiveFromISR(binAnySw, &xHigherPriorityAwoken);
		xSemaphoreGiveFromISR(binSw3, &xHigherPriorityAwoken);

		portEND_SWITCHING_ISR(xHigherPriorityAwoken); //wake up printing task
	}
}

void pinIntSetup() {
	/*Setup cf p. 163 in user manual*/
	Chip_PININT_Init(LPC_GPIO_PIN_INT); //Enable bit 18 in SYSAHBCLKCTRL0 cf t. 50
	Chip_INMUX_PinIntSel(0, SW1_port, SW1_pin); //write port-pin value to INTPIN bits in PINTSEL0-2 cf t. 131
	Chip_INMUX_PinIntSel(1, SW2_port, SW2_pin);
	Chip_INMUX_PinIntSel(2, SW3_port, SW3_pin);

	uint32_t pinChannels = PININTCH(0) | PININTCH(1) | PININTCH(2);

	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, pinChannels); //setting interrupt config to a known state
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, pinChannels); //set ISEL bits to 0 for level sensitive cf t. 153
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, pinChannels); //falling edge sensitive, cf t. 158

	NVIC_ClearPendingIRQ(PIN_INT0_IRQn); //finally, enabling interrupts in NVIC
	NVIC_ClearPendingIRQ(PIN_INT1_IRQn);
	NVIC_ClearPendingIRQ(PIN_INT2_IRQn);

	NVIC_EnableIRQ(PIN_INT0_IRQn);
	NVIC_EnableIRQ(PIN_INT1_IRQn);
	NVIC_EnableIRQ(PIN_INT2_IRQn);
}

/* the following is required if runtime statistics are to be collected */
void vConfigureTimerForRunTimeStats(void) {
	Chip_SCT_Init(LPC_SCTSMALL1);
	LPC_SCTSMALL1->CONFIG = SCT_CONFIG_32BIT_COUNTER;
	LPC_SCTSMALL1->CTRL_U = SCT_CTRL_PRE_L(255) | SCT_CTRL_CLRCTR_L; // set prescaler to 256 (255 + 1), and start timer
}
/* end runtime statictics collection */

}

/*setup of semaphores, event group, interrupts, iopins*/
void general_setup() {
	binAnySw = xSemaphoreCreateBinary();
	binSw1 = xSemaphoreCreateBinary();
	binSw2 = xSemaphoreCreateBinary();
	binSw3 = xSemaphoreCreateBinary();

	ITM_init();

	LpcPinMap none = { .port = -1, .pin = -1 }; // unused pin has negative values in it
	LpcPinMap txpin1 = { .port = 0, .pin = 18 }; // transmit pin that goes to Arduino D4
	LpcPinMap rxpin1 = { .port = 0, .pin = 13 }; // receive pin that goes to Arduino D3
	LpcUartConfig cfg1 = { .pUART = LPC_USART0, .speed = 115200, .data =
	UART_CFG_DATALEN_8 | UART_CFG_PARITY_NONE | UART_CFG_STOPLEN_1, .rs485 =
			false, .tx = txpin1, .rx = rxpin1, .rts = none, .cts = none };
	uart = new LpcUart(cfg1);

	sw1 = new DigitalIoPin(SW1_port, SW1_pin, DigitalIoPin::pullup, true);
	sw2 = new DigitalIoPin(SW2_port, SW2_pin, DigitalIoPin::pullup, true);
	sw3 = new DigitalIoPin(SW3_port, SW3_pin, DigitalIoPin::pullup, true);
	vTaskDelay(10);
	pinIntSetup();
}

void print_task_delay(int n, TickType_t delay) {
	char str[40]; //probably a better implementation would be to use a dedicated printing task
	sprintf(str, "Task %d, %lu ms waited\n\r", n, delay);

	uart->write(str);
}

#if EXER == 1
/*wait for button press*/
void vTask1(void *param) {
	general_setup();

	while (1) {
		xSemaphoreTake(binAnySw, portMAX_DELAY);
		xEventGroupSetBits(eGrp, 1);
	}
}

/*these wait for event bit, then fire randomly without stop*/
void vTask2(void *param) {
	srand(xTaskGetTickCount() + 1);
	TickType_t delay = 0;

	while (1) {
		xEventGroupWaitBits(eGrp, 1, pdFALSE, pdTRUE, portMAX_DELAY);
		delay = (TickType_t) rand() % 1000 + 1001;

		vTaskDelay(delay);
		print_task_delay(2, delay);
	}
}

void vTask3(void *param) {
	srand(xTaskGetTickCount() + 2);
	TickType_t delay = 0;

	while (1) {
		xEventGroupWaitBits(eGrp, 1, pdFALSE, pdTRUE, portMAX_DELAY);
		delay = (TickType_t) rand() % 1000 + 1001;

		vTaskDelay(delay);
		print_task_delay(3, delay);
	}
}

void vTask4(void *param) {
	srand(xTaskGetTickCount() + 3);
	TickType_t delay = 0;

	while (1) {
		xEventGroupWaitBits(eGrp, 1, pdFALSE, pdTRUE, portMAX_DELAY);
		delay = (TickType_t) rand() % 1000 + 1001;

		vTaskDelay(delay);
		print_task_delay(4, delay);
	}
}

#elif EXER == 2

uint32_t Tbits = T1_BIT | T2_BIT | T3_BIT;

void vTask1(void *param) {
	general_setup();

	int count = 0;
	TickType_t time = 0;

	xEventGroupSync(eGrp, T1_BIT, Tbits, portMAX_DELAY);
	while (1) {
		xSemaphoreTake(binSw1, portMAX_DELAY);
		count++;

		if (count == 1) {
			time = xTaskGetTickCount();
			xEventGroupSync(eGrp, T1_BIT, Tbits, portMAX_DELAY);
			print_task_delay(1, time);
			vTaskSuspend(NULL);
		}
	}
}

void vTask2(void *param) {
	int count = 0;
	TickType_t time = 0;

	xEventGroupSync(eGrp, T2_BIT, T1_BIT | T2_BIT | T3_BIT, portMAX_DELAY);
	while (1) {
		xSemaphoreTake(binSw2, portMAX_DELAY);
		count++;

		if (count == 2) {
			time = xTaskGetTickCount();
			xEventGroupSync(eGrp, T2_BIT, Tbits, portMAX_DELAY);
			print_task_delay(2, time);
			vTaskSuspend(NULL);
		}
	}
}

void vTask3(void *param) {
	int count = 0;
	TickType_t time = 0;

	xEventGroupSync(eGrp, T3_BIT, T1_BIT | T2_BIT | T3_BIT, portMAX_DELAY);
	while (1) {
		xSemaphoreTake(binSw3, portMAX_DELAY);
		count++;

		if (count == 3) {
			time = xTaskGetTickCount();
			xEventGroupSync(eGrp, T3_BIT, Tbits, portMAX_DELAY);
			print_task_delay(3, time);
			vTaskSuspend(NULL);
		}
	}
}
#elif EXER == 3

uint32_t Tbits = T1_BIT | T2_BIT | T3_BIT | T4_BIT;

void vTask1(void *param) {
	xEventGroupSync(eGrp, T1_BIT, Tbits, portMAX_DELAY);

	while (1) {
		xSemaphoreTake(binSw1, portMAX_DELAY); //binary semaphore is given on falling edge - no trigger from held button
		xEventGroupSetBits(eGrp, T1_BIT);
	}
}

void vTask2(void *param) {
	xEventGroupSync(eGrp, T2_BIT, Tbits, portMAX_DELAY);

	while (1) {
		xSemaphoreTake(binSw2, portMAX_DELAY);
		xEventGroupSetBits(eGrp, T2_BIT);
	}
}

void vTask3(void *param) {
	xEventGroupSync(eGrp, T3_BIT, Tbits, portMAX_DELAY);

	while (1) {
		xSemaphoreTake(binSw3, portMAX_DELAY);
		xEventGroupSetBits(eGrp, T3_BIT);
	}
}

void vTask4(void *param) {
	general_setup();

	TickType_t last = xTaskGetTickCount();
	TickType_t time = xTaskGetTickCount();
	uint32_t taskBits = 0;
	char str[80];

	xEventGroupSync(eGrp, T4_BIT, Tbits, portMAX_DELAY); //Sync task start
	while (1) {
		taskBits = xEventGroupSync(eGrp, T4_BIT, Tbits,
		configTICK_RATE_HZ * 5); //sync button press
		time = xTaskGetTickCount();

		if (taskBits == Tbits) {
			sprintf(str, "OK\r\nTicks elapsed: %lu\r\n", time - last);
			last = time;
			uart->write(str);
		} else { //not all bits were set - not all tasks synchronised in time
			uart->write("Fail\r\n");
			for (int i = 1; i < 4; i++) {
				if (!(taskBits >> i & 1)) { //check if i'th bit is set
					sprintf(str, "Task %d timed out after %lu ticks\r\n", i,
							time - last);
					uart->write(str);
				}
			}

			vTaskSuspend(NULL);
		}
	}
}

#endif
int main(void) {
	prvSetupHardware();

#if EXER == 1
	xTaskCreate(vTask1, "Notify", configMINIMAL_STACK_SIZE * 2, NULL,
	tskIDLE_PRIORITY + 2UL, NULL);

	xTaskCreate(vTask2, "Print1", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask3, "Print2", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask4, "Print3", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
#elif EXER == 2
	xTaskCreate(vTask1, "sw1 monitor", configMINIMAL_STACK_SIZE * 2, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask2, "sw2 monitor", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask3, "sw3 monitor", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
#elif EXER == 3

	xTaskCreate(vTask1, "Listen1", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask2, "Listen2", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask3, "Listen3", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask4, "Watchdog", configMINIMAL_STACK_SIZE * 2, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
#endif
	eGrp = xEventGroupCreate();
	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

