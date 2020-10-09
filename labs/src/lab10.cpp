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

#define EXER 3

#define STR_SIZE 20
#define SBF 80 //switch bounce filter
#define T1_BIT 1 << 1
#define T2_BIT 1 << 2
#define T3_BIT 1 << 3
#define T4_BIT 1 << 4

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "timers.h"

#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"
#include "ITM_write.h"
#include "LpcUart.h"

#include "pin_constants.h"
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
QueueHandle_t printQ;
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

	/*pin interrupt setup of sw1-sw3*/
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
/* end runtime statictics collection */

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
}

#if EXER == 1
const uint32_t tbits = T1_BIT | T2_BIT;
TimerHandle_t osTimer;
TimerHandle_t reTimer;
SemaphoreHandle_t binOSt;

void timerCallback(TimerHandle_t xTimer) {
	int id = (int) pvTimerGetTimerID(xTimer);
	if (id == 0)
		xSemaphoreGive(binOSt);
	else
		xQueueSend(printQ, "hello\r\n", portMAX_DELAY);
}

void vTask1(void *param) {
	general_setup();
	printQ = xQueueCreate(4, sizeof(char[STR_SIZE]));
	binOSt = xSemaphoreCreateBinary();

	osTimer = xTimerCreate("1-shotTimer", configTICK_RATE_HZ * 20, pdFALSE,
			(void*) 0, timerCallback);
	reTimer = xTimerCreate("reTimer", configTICK_RATE_HZ * 5, pdTRUE, (void*) 1,
			timerCallback);

	char str[STR_SIZE] = "";

	xEventGroupSync(eGrp, T1_BIT, tbits, portMAX_DELAY);
	uart->write("starting\r\n");
	xTimerStart(osTimer, portMAX_DELAY);
	xTimerStart(reTimer, portMAX_DELAY);
	while (1) {
		xQueueReceive(printQ, &str, portMAX_DELAY);

		uart->write(str);
	}
}

void vTask2(void *param) {
	xEventGroupSync(eGrp, T2_BIT, tbits, portMAX_DELAY);

	while (1) {
		xSemaphoreTake(binOSt, portMAX_DELAY);
		xQueueSend(printQ, "aargh\r\n", portMAX_DELAY);
	}
}

#elif EXER == 2
TimerHandle_t osTimer;

void timerCallback(TimerHandle_t xTimer) {
	Board_LED_Set(1, false);
}

void vTask1(void *param) {
	general_setup();
	osTimer = xTimerCreate("1-shotTimer", configTICK_RATE_HZ * 5, pdFALSE,
			(void*) 0, timerCallback);

	while (1) {
		xSemaphoreTake(binAnySw, portMAX_DELAY);
		xTimerStart(osTimer, portMAX_DELAY); //start or reset duration of timer
		//xTimerStart is equal to xTimerReset for timers that are already active
		Board_LED_Set(1, true);
	}
}
#elif EXER == 3
const uint32_t timeoutBit = 1 << 0;
TimerHandle_t timeout;

void timerCallback(TimerHandle_t xTimer) {
	xEventGroupClearBits(eGrp, timeoutBit);
}

void vTask1(void *param) {
	general_setup();
	xEventGroupSetBits(eGrp, timeoutBit);
	timeout = xTimerCreate("Rx timeout", configTICK_RATE_HZ * 30, pdFALSE,
			(void*) 0, timerCallback);

	char str[STR_SIZE];
	char c;
	int i;

	uart->write("Ready\r\n");
	while (1) {
		i = 0;
		c = 0;
		/*receive single character until linefeed*/
		while ((c != '\n' && c != '\r') && i < STR_SIZE - 1) {
			if (xEventGroupGetBits(eGrp) & timeoutBit) {
				//no timeout from timerCallback
				if (uart->read(&c, 1, portMAX_DELAY)) {
					str[i] = c;
					i++;
					uart->write(c);
					xEventGroupSetBits(eGrp, timeoutBit);
					xTimerStart(timeout, 0);
				}
			} else { //timeout. Reset input buffer
				i = 0;
				uart->write("[Inactive]\r\n");
				xEventGroupSetBits(eGrp, timeoutBit);
			}
		}
		ITM_write("string received\n");
		uart->write("\r\n");
		str[i] = '\0';
	}
}
#endif

int main(void) {
	prvSetupHardware();

	eGrp = xEventGroupCreate();
#if EXER == 1
	xTaskCreate(vTask1, "waitQ", configMINIMAL_STACK_SIZE * 3, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
	xTaskCreate(vTask2, "waitBin", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
#elif EXER == 2
	xTaskCreate(vTask1, "task1", configMINIMAL_STACK_SIZE * 3, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
#elif EXER == 3
	xTaskCreate(vTask1, "task1", configMINIMAL_STACK_SIZE * 3, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
#endif

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

