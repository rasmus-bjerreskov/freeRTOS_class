/*
 * SMutex.cpp
 *
 *  Created on: 29 Aug 2020
 *      Author: Rasmus
 */

#include "SMutex.h"

Smutex::Smutex() {
	mutex = xSemaphoreCreateMutex();

}

Smutex::~Smutex() {
	// TODO Auto-generated destructor stub
}

void Smutex::lock(){
	xSemaphoreTake(mutex, portMAX_DELAY);
}

void Smutex::unlock(){
	xSemaphoreGive(mutex);
}

