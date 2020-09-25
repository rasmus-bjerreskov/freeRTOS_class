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
#include "LpcUart.h"
#include "ITM_write.h"
#include "queue.h"
#include "semphr.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

SemaphoreHandle_t instr;
SemaphoreHandle_t sbRIT;
SemaphoreHandle_t go;

volatile uint32_t RIT_count;
volatile bool do_step = true;

DigitalIoPin *dir;
DigitalIoPin *step;
DigitalIoPin *lim1;
DigitalIoPin *lim2;
DigitalIoPin *btn1;
DigitalIoPin *btn2;
DigitalIoPin *btn3;

enum code_t {
	MOVE, PPS, STOP, ERR
};
enum dir_t {
	RIGHT, LEFT
};
typedef struct { //TODO: maybe trim this down to just one int and one enum
	int cnt = 0;
	int pps = 400;
	code_t code = PPS;
	dir_t dir = LEFT;
} instruct_t;

instruct_t curCmd;
/*****************************************************************************
 * Private functions
 ****************************************************************************/

/* Sets up system hardware */
static void prvSetupHardware(void) {
	SystemCoreClockUpdate();
	Board_Init();
	heap_monitor_setup();

	// initialize RIT (= enable clocking etc.)
	Chip_RIT_Init(LPC_RITIMER);
	// set the priority level of the interrupt
	// The level must be equal or lower than the maximum priority specified in FreeRTOS config
	// Note that in a Cortex-M3 a higher number indicates lower interrupt priority
	NVIC_SetPriority(RITIMER_IRQn,
	configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);

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

bool atLimit() {
	bool ret;
	if (curCmd.dir == LEFT) {
		if (ret = lim1->read())
			Board_LED_Set(0, true);
	} else {
		if (ret = lim2->read())
			Board_LED_Set(1, true);
	}

	if (ret)
		ITM_write("Limit hit\n");

	return ret;
}

extern "C" {
void RIT_IRQHandler(void) {
// This used to check if a context switch is required
	portBASE_TYPE xHigherPriorityWoken = pdFALSE;
// Tell timer that we have processed the interrupt.
// Timer then removes the IRQ until next match occurs
	Chip_RIT_ClearIntStatus(LPC_RITIMER); // clear IRQ flag
	if (!atLimit() && RIT_count > 0) {
		step->write(do_step);
		do_step = !do_step;
		if (do_step) {
			RIT_count--; //high->low->decrement
		}
	} else {
		step->write(false);
		do_step = true; //resetting in case limit switch trigger while pin is set high
		Chip_RIT_Disable(LPC_RITIMER); // disable timer
// Give semaphore and set context switch flag if a higher priority task was woken up
		xSemaphoreGiveFromISR(sbRIT, &xHigherPriorityWoken);
	}
// End the ISR and (possibly) do a context switch
	portEND_SWITCHING_ISR(xHigherPriorityWoken);
}
}

void RIT_start(int count, int hZ) {
	uint64_t cmp_value;
// Determine approximate compare value based on clock rate and passed interval
	cmp_value = (uint64_t) Chip_Clock_GetSystemClockRate()
			/ (uint64_t) (hZ * 2); //*2 to get on/off signal per pulse
// disable timer during configuration
	Chip_RIT_Disable(LPC_RITIMER);
	RIT_count = count;
// enable automatic clear on when compare value==timer value
// this makes interrupts trigger periodically
	Chip_RIT_EnableCompClear(LPC_RITIMER);
// reset the counter
	Chip_RIT_SetCounter(LPC_RITIMER, 0);
	Chip_RIT_SetCompareValue(LPC_RITIMER, cmp_value);
// start counting
	Chip_RIT_Enable(LPC_RITIMER);
// Enable the interrupt signal in NVIC (the interrupt controller)
	NVIC_EnableIRQ(RITIMER_IRQn);
// wait for ISR to tell that we're done
	if (xSemaphoreTake(sbRIT, portMAX_DELAY) == pdTRUE) {
// Disable the interrupt signal in NVIC (the interrupt controller)
		NVIC_DisableIRQ(RITIMER_IRQn);
	} else {
// unexpected error
	}
}

static void vTask1(void *pvParameters) {
	ITM_init();

	sbRIT = xSemaphoreCreateBinary();
	go = xSemaphoreCreateBinary();
	instr = xQueueCreate(10, sizeof(instruct_t));

	LpcPinMap none = { .port = -1, .pin = -1 }; // unused pin has negative values in it
	LpcPinMap txpin1 = { .port = 0, .pin = 18 }; // transmit pin that goes to Arduino D4
	LpcPinMap rxpin1 = { .port = 0, .pin = 13 }; // receive pin that goes to Arduino D3
	LpcUartConfig cfg1 =
			{ .pUART = LPC_USART0, .speed = 115200, .data = UART_CFG_DATALEN_8
					| UART_CFG_PARITY_NONE | UART_CFG_STOPLEN_1, .rs485 = false,
					.tx = txpin1, .rx = rxpin1, .rts = none, .cts = none };
	LpcUart uart(cfg1);
	const int maxlen = 80;
	char str[maxlen];
	char c;
	int i;

	instruct_t cmd;

	ITM_write("UART ready\n");

	while (1) {
		i = 0;
		c = 0;
		while ((c != '\n' && c != '\r') && i < maxlen - 1) {
			if (uart.read(&c, 1, configTICK_RATE_HZ / 1000)) {
				str[i] = c;
				i++;
				uart.write(c);
			}
		}
		ITM_write("done\n");
		uart.write("\r\n");
		str[i] = '\0';
		ITM_write(str);

		if (strncmp(str, "left ", 5) == 0) {
			cmd.cnt = atoi(&str[5]);
			cmd.code = MOVE;
			cmd.dir = LEFT;
			ITM_write("dir: left\n");
		} else if (strncmp(str, "right ", 6) == 0) {
			cmd.cnt = atoi(&str[6]);
			cmd.code = MOVE;
			cmd.dir = RIGHT;
			ITM_write("dir: right\n");
		} else if (strncmp(str, "pps ", 4) == 0) {
			cmd.pps = atoi(&str[4]);
			cmd.code = PPS;
			ITM_write("pps set\n");
		} else if (strncmp(str, "stop", 4) == 0) {
			cmd.code = STOP;
		} else if (strncmp(str, "go", 2) == 0) {
			cmd.code = ERR; //this is not a command to go on the queue
			xSemaphoreGive(go);
		} else {
			ITM_write("Invalid command\n");
			cmd.code = ERR;
		}

		if (cmd.code != ERR) {
			BaseType_t qState;
			if (cmd.code == STOP)
				qState = xQueueSendToFront(instr, (void* )&cmd,
						configTICK_RATE_HZ * 2);
			else
				qState = xQueueSend(instr, (void* )&cmd,
						configTICK_RATE_HZ * 2);

			if (qState == errQUEUE_FULL)
				uart.write("Queue full\r\n");
		}
	}
}

static void vTask2(void *pvParameters) {
	step = new DigitalIoPin(0, 24, DigitalIoPin::output, true);
	dir = new DigitalIoPin(1, 0, DigitalIoPin::output, true);
	lim1 = new DigitalIoPin(0, 27, DigitalIoPin::pullup, true);
	lim2 = new DigitalIoPin(0, 28, DigitalIoPin::pullup, true);
	btn1 = new DigitalIoPin(0, 8, DigitalIoPin::pullup, true);
	btn2 = new DigitalIoPin(1, 6, DigitalIoPin::pullup, true);
	btn3 = new DigitalIoPin(1, 8, DigitalIoPin::pullup, true);
	dir->write(true);
	char str[80];
	vTaskDelay(300);

	xSemaphoreTake(go, portMAX_DELAY);
	while (1) {
		xQueueReceive(instr, &curCmd, portMAX_DELAY);
		ITM_write("Handling command\n");
		switch (curCmd.code) {
		case MOVE:
			sprintf(str, "steps: %d\n", curCmd.cnt);
			break;
		case PPS:
			sprintf(str, "pps: %d\n", curCmd.pps);
			break;
		case STOP:
			sprintf(str, "Halting\n");
			break;
		default:
			sprintf(str, "Unexpected error\n");
			break;
		}
		ITM_write(str);

		if (curCmd.code == STOP)
			xSemaphoreTake(go, portMAX_DELAY);
		if (curCmd.code == MOVE && curCmd.pps > 0) { //soft lock/infinite loop will happen if we try and make the RItimer count to 0
			if (!atLimit()) {
				Board_LED_Set(0, false);
				Board_LED_Set(1, false);
			}
			dir->write(curCmd.dir);
			RIT_start(curCmd.cnt, curCmd.pps);
		}

	}
}

int main(void) {
	prvSetupHardware();

	xTaskCreate(vTask1, "task1", configMINIMAL_STACK_SIZE * 4, NULL,
	tskIDLE_PRIORITY + 1, NULL);

	xTaskCreate(vTask2, "task2", configMINIMAL_STACK_SIZE * 4, NULL,
	tskIDLE_PRIORITY + 1, NULL);

	/* Start the scheduler */
	vTaskStartScheduler();

	/* Should never arrive here */
	return 1;
}

