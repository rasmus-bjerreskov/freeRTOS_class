/*
 * SMutex.h
 *
 *  Created on: 29 Aug 2020
 *      Author: Rasmus
 */

#include "FreeRTOS.h"
#include "semphr.h"

#ifndef SMUTEX_H_
#define SMUTEX_H_

class Smutex {
public:
	Smutex();
	virtual ~Smutex();
	void lock();
	void unlock();
private:
	SemaphoreHandle_t mutex;
};

#endif /* SMUTEX_H_ */
