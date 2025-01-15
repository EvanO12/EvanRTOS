/*
 * eos_semaphore.h
 *
 *  Created on: Jan 10, 2025
 *      Author: EO12
 */

#ifndef INC_EOS_SEMAPHORE_H_
#define INC_EOS_SEMAPHORE_H_

#include "eos_kernel.h"

/*	DATATYPES	*/

typedef struct  {
	volatile uint32_t count;
	uint32_t max_count;
} EOS_semaphore_t;

typedef EOS_semaphore_t *EOS_semaphore_id_t;


/*	FUNCTION DECLARATIONS	*/
EOS_status_t EOS_SemaphoreAcquire(EOS_semaphore_id_t semaphore);
EOS_status_t EOS_SemaphoreRelease(EOS_semaphore_id_t semaphore);
EOS_semaphore_id_t EOS_SemaphoreNew(uint8_t count);


#endif /* INC_EOS_SEMAPHORE_H_ */
