/*
 * eos_queue.h
 *
 *  Created on: Jan 10, 2025
 *      Author: EO12
 */

#ifndef INC_EOS_QUEUE_H_
#define INC_EOS_QUEUE_H_

#include "eos_kernel.h"

/*	DATATYPES	*/

typedef struct {
	void *buffer;
	uint32_t head;
	uint32_t tail;
	uint32_t size;
	uint32_t item_size;
	volatile uint32_t count;
} EOS_queue_t;

typedef EOS_queue_t* EOS_queue_id_t;



/*	FUNCTION DECLARATIONS	*/
EOS_queue_id_t EOS_QueueCreate(uint32_t size, uint32_t item_size);
EOS_status_t EOS_QueueGet(EOS_queue_id_t queue, void *item, EOS_block_status_t block);
EOS_status_t EOS_QueuePut(EOS_queue_id_t queue, const void *item, EOS_block_status_t block);

#endif /* INC_EOS_QUEUE_H_ */
