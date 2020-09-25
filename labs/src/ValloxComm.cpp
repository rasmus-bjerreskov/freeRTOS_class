/*
 * ValloxComm.cpp
 *
 *  Created on: 22.1.2020
 *      Author: keijo
 */
#include "board.h"

#include "LpcUart.h"


void vValloxTask(void *pvParameters)
{
	LpcPinMap none = { .port = -1, .pin = -1}; // unused pin has negative values in it
	LpcPinMap txpin1 = { .port = 0, .pin = 10 }; // transmit pin that goes to Arduino D4
	LpcPinMap rxpin1 = { .port = 0, .pin = 9 }; // receive pin that goes to Arduino D3
	LpcUartConfig cfg1 = {
			.pUART = LPC_USART0,
			.speed = 9600,
			.data = UART_CFG_DATALEN_8 | UART_CFG_PARITY_NONE | UART_CFG_STOPLEN_1,
			.rs485 = false,
			.tx = txpin1,
			.rx = rxpin1,
			.rts = none,
			.cts = none
	};
	LpcUart vallox(cfg1);
	char str[80];
	int count = 0;

	/* Set up SWO to PIO1_2 to enable ITM */
	Chip_SWM_MovablePortPinAssign(SWM_SWO_O, 1, 2);
	iprt("Vallox\r\n");

	while (1) {
		count = vallox.read(str, 80, portTICK_PERIOD_MS * 100);
		if(count > 0) {
			processData(str, count);
		}
		else {
			/* receive timed out */
		}
	}
}



