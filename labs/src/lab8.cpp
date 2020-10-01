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

#include <cstring>
#include <cstdlib>

#include "FreeRTOS.h"
#include "task.h"
#include "heap_lock_monitor.h"
#include "DigitalIoPin.h"
#include "pin_constants.h"
#include "queue.h"
#include "ITM_write.h"
#include "LpcUart.h"

#define EXER 3
/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

DigitalIoPin *sw1;
DigitalIoPin *sw2;
DigitalIoPin *sw3;
LpcUart *uart;

QueueHandle_t cntQ;
QueueHandle_t filterQ;
QueueHandle_t ledQ;

enum led_t {
	red, green, blue
};

typedef struct {
	TickType_t time;
	int sw_n;
} press_time_t;
/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

extern "C" {
#if EXER == 1

void PIN_INT0_IRQHandler(void) {
	ITM_write("SW1");
	static int n = 1; //switch id
	BaseType_t xHigherPriorityAwoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);

	xQueueSendFromISR(cntQ, &n, &xHigherPriorityAwoken);

	portEND_SWITCHING_ISR(xHigherPriorityAwoken); //wake up printing task
}

void PIN_INT1_IRQHandler(void) {
	ITM_write("SW2");
	static int n = 2; //switch id
	BaseType_t xHigherPriorityAwoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

	xQueueSendFromISR(cntQ, &n, &xHigherPriorityAwoken);

	portEND_SWITCHING_ISR(xHigherPriorityAwoken); //wake up printing task
}

void PIN_INT2_IRQHandler(void) {
	ITM_write("SW3");
	static int n = 3; //switch id
	BaseType_t xHigherPriorityAwoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);

	xQueueSendFromISR(cntQ, &n, &xHigherPriorityAwoken);

	portEND_SWITCHING_ISR(xHigherPriorityAwoken); //wake up printing task
}

#elif EXER == 2 || EXER == 3

void PIN_INT0_IRQHandler(void) {
	//ITM_write("SW1");
	static press_time_t t1 { 0, 1 };
	t1.time = xTaskGetTickCountFromISR();

	BaseType_t xHigherPriorityAwoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);

	xQueueSendFromISR(cntQ, &t1, &xHigherPriorityAwoken);

	portEND_SWITCHING_ISR(xHigherPriorityAwoken); //wake up printing task
}

void PIN_INT1_IRQHandler(void) {
	//ITM_write("SW2");
	static press_time_t t2 { 0, 2 };
	t2.time = xTaskGetTickCountFromISR();

	BaseType_t xHigherPriorityAwoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);

	xQueueSendFromISR(cntQ, &t2, &xHigherPriorityAwoken);

	portEND_SWITCHING_ISR(xHigherPriorityAwoken); //wake up printing task
}

void PIN_INT2_IRQHandler(void) {
	//ITM_write("SW3");
	static press_time_t t3 { 0, 3 };
	t3.time = xTaskGetTickCountFromISR();

	BaseType_t xHigherPriorityAwoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH2);

	xQueueSendFromISR(cntQ, &t3, &xHigherPriorityAwoken);

	portEND_SWITCHING_ISR(xHigherPriorityAwoken); //wake up printing task
}

#endif
#if EXER == 3
//rising edge
void PIN_INT3_IRQHandler() {
	ITM_write("SW3 rising");
	static press_time_t t4 { 0, 4 };
	t4.time = xTaskGetTickCountFromISR();

	BaseType_t xHigherPriorityAwoken = pdFALSE;
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH3);

	xQueueSendFromISR(cntQ, &t4, &xHigherPriorityAwoken);

	portEND_SWITCHING_ISR(xHigherPriorityAwoken);
}
#endif
}

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
/*Setup SW1-SW3 as falling edge pin interrupts*/
void pinIntSetup() {
	/*Setup cf p. 163 in user manual*/
	Chip_PININT_Init(LPC_GPIO_PIN_INT); //Enable bit 18 in SYSAHBCLKCTRL0 cf t. 50
	Chip_INMUX_PinIntSel(0, SW1_port, SW1_pin); //write port-pin value to INTPIN bits in PINTSEL0-2 cf t. 131
	Chip_INMUX_PinIntSel(1, SW2_port, SW2_pin);
	Chip_INMUX_PinIntSel(2, SW3_port, SW3_pin);
	Chip_INMUX_PinIntSel(3, SW3_port, SW3_pin); //fourth interrupt needed for exercise 3

	uint32_t pinChannels = PININTCH(0) | PININTCH(1) | PININTCH(2);

	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, pinChannels); //setting interrupt config to a known state
	Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(3));
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, pinChannels); //set ISEL bits to 0 for level sensitive cf t. 153
	Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(3));
	Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, pinChannels); //falling edge sensitive, cf t. 158
	Chip_PININT_EnableIntHigh(LPC_GPIO_PIN_INT, PININTCH(3)); //rising edge sensitive cf t. 155

	NVIC_ClearPendingIRQ(PIN_INT0_IRQn); //finally, enabling interrupts in NVIC
	NVIC_ClearPendingIRQ(PIN_INT1_IRQn);
	NVIC_ClearPendingIRQ(PIN_INT2_IRQn);
	NVIC_ClearPendingIRQ(PIN_INT3_IRQn);

	NVIC_EnableIRQ(PIN_INT0_IRQn);
	NVIC_EnableIRQ(PIN_INT1_IRQn);
	NVIC_EnableIRQ(PIN_INT2_IRQn);
	NVIC_EnableIRQ(PIN_INT3_IRQn);
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

#if EXER == 1

void vTask1(void *pvParamters) {
	ITM_init();
	sw1 = new DigitalIoPin(SW1_port, SW1_pin, DigitalIoPin::pullup, true);
	sw2 = new DigitalIoPin(SW2_port, SW2_pin, DigitalIoPin::pullup, true);
	sw3 = new DigitalIoPin(SW3_port, SW3_pin, DigitalIoPin::pullup, true);
	vTaskDelay(10); //waiting on sw2 to settle
	cntQ = xQueueCreate(2, sizeof(int));

	pinIntSetup();

	int curSw = 0;
	int prevSw = 0;
	int presses = 0;
	char str[40];

	vTaskDelay(100);
	while (1) {
		xQueueReceive(cntQ, &curSw, portMAX_DELAY);


		if (prevSw && curSw != prevSw) { //check that this is not the first button pressed and not the same as last press
			sprintf(str, "\nButton %d pressed %d times.\n", prevSw, presses);
			ITM_write(str);
			presses = 0;
		}

		presses++;
		prevSw = curSw;
	}
}

#elif EXER == 2
void vTask1(void *pvParameters) {
	ITM_init();
	sw1 = new DigitalIoPin(SW1_port, SW1_pin, DigitalIoPin::pullup, true);
	sw2 = new DigitalIoPin(SW2_port, SW2_pin, DigitalIoPin::pullup, true);
	sw3 = new DigitalIoPin(SW3_port, SW3_pin, DigitalIoPin::pullup, true);
	vTaskDelay(10);
	cntQ = xQueueCreate(2, sizeof(press_time_t));

	pinIntSetup();

	press_time_t tbuf;
	TickType_t lastT = 0;
	TickType_t filter = 50;
	uint32_t deltaT;
	char str[80];
	vTaskDelay(100);
	uart->write("Ready\r\n");
	while (1) {
		xQueueReceive(cntQ, &tbuf, portMAX_DELAY); //wait for presses
		xQueueReceive(filterQ, &filter, configTICK_RATE_HZ / 1000); //check for new filter values

		deltaT = tbuf.time - lastT;

		if (deltaT >= filter) { //accepted value
			if (deltaT >= 1000) { //prettier formatting of long deltas
				float tmp = (float) deltaT / 1000;
				sprintf(str, "%.1f s Button %d\r\n", tmp, tbuf.sw_n);
			} else {
				sprintf(str, "%lu ms Button %d\r\n", deltaT, tbuf.sw_n);
			}
			uart->write(str);
		}
		lastT = tbuf.time;
	}
}

void vTask2(void *pvParameters) {
	filterQ = xQueueCreate(1, sizeof(int));

	LpcPinMap none = { .port = -1, .pin = -1 }; // unused pin has negative values in it
	LpcPinMap txpin1 = { .port = 0, .pin = 18 }; // transmit pin that goes to Arduino D4
	LpcPinMap rxpin1 = { .port = 0, .pin = 13 }; // receive pin that goes to Arduino D3
	LpcUartConfig cfg1 = { .pUART = LPC_USART0, .speed = 115200, .data =
	UART_CFG_DATALEN_8 | UART_CFG_PARITY_NONE | UART_CFG_STOPLEN_1, .rs485 =
			false, .tx = txpin1, .rx = rxpin1, .rts = none, .cts = none };
	uart = new LpcUart(cfg1);
	const int maxlen = 30;
	char str[maxlen];
	char c;
	int i;

	int filter;

	vTaskDelay(100);
	while (1) {
		i = 0;
		c = 0;
		/*receive single character until linefeed*/
		while ((c != '\n' && c != '\r') && i < maxlen - 1) {
			if (uart->read(&c, 1, portMAX_DELAY)) {
				str[i] = c;
				i++;
				uart->write(c);
			}
		}
		ITM_write("string received\n");
		uart->write("\r\n");
		str[i] = '\0';

		if (strncmp(str, "filter ", 7) == 0) { //send filter value
			filter = atoi(&str[7]);
			sprintf(str, "New filter value: %d ms\r\n", filter);
			uart->write(str);
			xQueueSend(filterQ, &filter, portMAX_DELAY);
		} else
			uart->write("Bad input\r\n");
	}
}


#elif EXER == 3
void vTask1(void *pvParameters) {
	ITM_init();
	sw1 = new DigitalIoPin(SW1_port, SW1_pin, DigitalIoPin::pullup, true);
	sw2 = new DigitalIoPin(SW2_port, SW2_pin, DigitalIoPin::pullup, true);
	sw3 = new DigitalIoPin(SW3_port, SW3_pin, DigitalIoPin::pullup, true);
	vTaskDelay(10);
	cntQ = xQueueCreate(2, sizeof(press_time_t));

	pinIntSetup();

	press_time_t tbuf;
	TickType_t lastT = 0;
	const TickType_t filter = 50;
	TickType_t sw3_start;

	enum state_t {
		read, sw3Held, config
	};
	state_t state = read;
	uint8_t code = 0b11110110;
	uint8_t input = 0;	//entered code
	int count = 0; //config digit count

	char str[40];
	while (1) {
		xQueueReceive(cntQ, &tbuf, portMAX_DELAY);
		uint32_t deltaT = tbuf.time - lastT;
		lastT = tbuf.time;

		if (deltaT >= filter) {

			if (state == read) {		//state: currently testing code
				if (deltaT >= 15000) //user taking too long, clear input
					input = 0;

				if (tbuf.sw_n == 1 || tbuf.sw_n == 2) {
					input = (tbuf.sw_n - 1) | input << 1; //add press to input buffer
					sprintf(str, "%d", tbuf.sw_n - 1);
					ITM_write(str);
					if (input == code) {
						led_t tmp = green;
						xQueueSend(ledQ, &tmp, portMAX_DELAY);
					}

				} else if (tbuf.sw_n == 4) {
					state = sw3Held;
					sw3_start = tbuf.time;
				}

			} else if (state == sw3Held) { //state: sw3 currently held
				if (tbuf.sw_n == 3) { //all other input should be ignored while sw3 held
					if (tbuf.time - sw3_start < 3000) //button released too soon
						state = read;
				} else if (tbuf.time - sw3_start >= 3000) {
					state = config;
					ITM_write("\nEnter new code\n");
					led_t tmp = blue;
					xQueueSend(ledQ, &tmp, portMAX_DELAY);

				}
			} else if (state == config) { //state: setting new code
				if (tbuf.sw_n == 1 || tbuf.sw_n == 2) {
					count++;
					code = (tbuf.sw_n - 1) | code << 1;
					sprintf(str, "%d", tbuf.sw_n - 1);
					ITM_write(str);
					if (count == 8) { //done entering new code
						count = 0;
						state = read;
						ITM_write("\nNew code accepted\n");
						led_t tmp = red;
						xQueueSend(ledQ, &tmp, portMAX_DELAY);
					}
				}
			}
		}
	}
}

void vTask2(void *pvParameters) { //second task for managing led timers. Maybe overkill to create a task for this, but it simplifies the code of task1
	ledQ = xQueueCreate(1, sizeof(int));

	const TickType_t greenT = 5000;
	led_t led = red;
	TickType_t prevT;
	while (1) {
		if (xQueueReceive(ledQ, &led, 0) == pdTRUE) {
			if (led == green)
				prevT = xTaskGetTickCount();
		}
		if (led == green && xTaskGetTickCount() - prevT >= greenT)
			led = red;

		switch (led) {
		case red:
			Board_LED_Set(0, true);
			Board_LED_Set(1, false);
			Board_LED_Set(2, false);
			break;

		case green:
			Board_LED_Set(0, false);
			Board_LED_Set(1, true);
			Board_LED_Set(2, false);
			break;

		case blue:
			Board_LED_Set(0, false);
			Board_LED_Set(1, false);
			Board_LED_Set(2, true);
			break;
		}
		vTaskDelay(1);
	}
}
#endif
int main(void) {
	prvSetupHardware();

	xTaskCreate(vTask1, "printTask", configMINIMAL_STACK_SIZE * 3, NULL,
	tskIDLE_PRIORITY + 2UL, NULL);

#if EXER == 2
	xTaskCreate(vTask2, "readTask", configMINIMAL_STACK_SIZE * 3, NULL,
	tskIDLE_PRIORITY + 3UL, NULL);
#endif

#if EXER == 3
	xTaskCreate(vTask2, "ledTask", configMINIMAL_STACK_SIZE, NULL,
	tskIDLE_PRIORITY + 1UL, NULL);
#endif
	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

