/*
 * eos_queue.c
 *
 *
 *
 *      EvanRTOS uses a FIFO based queue implementation. The queue is of fixed size, and will act as a circular buffer in the sense that
 *      head and tail pointers wrap around when the exceed the length of the queue. The tail pointer points to the position of where
 *      the next item is to be added, while the head points to next item to be removed from the queue.
 *
 *      EvanRTOS queues currently support the following operations:
 *      	EOS_QueueCreate();
 *      	EOS_QueueGet();
 *      	EOS_QueuePut();
 *
 *      EOS_QueueGet and EOS_QueuePut can be called from interrupt contexts by adding EOS_block_status_t EOS_NO_BLOCK as the third parameter.
 *      This will return EOS_BLOCKED if the queue is full, and a message cannot be added, or if the queue is empty, and a message cannot be
 *      retrieved. Otherwise, EOS_OK will be returned on success of the operation. By using EOS_block_status_t EOS_BLOCK,the put and get
 *      functions can be called from tasks. The tasks will attempt the operation, and enter a blocked state if they cannot yet be executed.
 *
 *      Queues are represented by the EOS_queue_t datatype. Like semaphores, when a queue is created, a id is returned. This is the EOS_queue_id_t
 *      type which is a pointer to the queue under the hood. For users of EvanRTOS, this can be treated as an id to be passed into the get
 *      and put functions.
 *
 *      All queues in EvanRTOS are dynamically allocated.
 *
 *     	It should be noted the EvanRTOS queues currently unblock tasks in a supoptimal way, which might lead to issues when using multiple
 *     	producers and consumers on queues of small sizes. This will be fixed soon.
 *
 */


/*	INCLUDES	*/
#include "eos_queue.h"



/*	QUEUE FUNCTIONALITY		*/


/**
 * @brief Creates a new queue
 *
 * @param size The max number of items the queue can hold
 * @param item_size The size of each item in the queue, in bytes
 * @return ID of queue
 */
EOS_queue_id_t EOS_QueueCreate(uint32_t size, uint32_t item_size){

	EOS_queue_t *queue = (EOS_queue_t *)malloc(sizeof(EOS_queue_t));
	    if (queue == NULL)
	    {
	        return NULL;
	    }
	queue->buffer = malloc(item_size * size);

	if (queue->buffer == NULL)
	{
		free(queue);
		return NULL;
	}

	queue->head = 0;
	queue->tail = 0;
	queue->size = size;
	queue->item_size = item_size;
	queue->count = 0;
	return queue;

}

/**
 * @brief Retrieves an item from the specified queue.
 *
 * @param queue         Pointer to the queue from which an item is to be retrieved.
 * @param item          Pointer to the memory where the dequeued item will be copied.
 * @param block         Blocking behavior of the function:
 *                      - `EOS_BLOCK`: If the queue is empty, the calling task will block until an item is available.
 *                      - `EOS_NO_BLOCK`: If the queue is empty, the function will return immediately with `EOS_BLOCKED`.
 *
 * @return              Status of the operation:
 *                      - `EOS_OK`: Item was successfully retrieved.
 *                      - `EOS_BLOCKED`: Queue is empty, and blocking is disabled.
 *
 * @todo Improve the task unblocking method.
 */
EOS_status_t EOS_QueueGet(EOS_queue_id_t queue, void *item, EOS_block_status_t block){

	EOS_EnterCritical();

	if (queue->count == 0)
	{
		if(block == EOS_BLOCK)
		{
		run_ptr->blocked = (void *)queue;
		EOS_ExitCritical();
		EOS_Suspend();
		EOS_EnterCritical();

			while (queue->count == 0)
			{
				EOS_ExitCritical();
				EOS_Suspend();
				EOS_EnterCritical();
			}
		}

		else
		{	EOS_ExitCritical();
			return EOS_BLOCKED;
		}
	}

	void *src = (uint8_t *)queue->buffer + (queue->head * queue->item_size);
	memcpy(item, src, queue->item_size);
	queue->head = (queue->head + 1) % queue->size;
	queue->count--;

	EOS_TaskUnblock(queue);

	EOS_ExitCritical();
	return EOS_OK;

}

/**
 * @brief Adds an item to the specified queue
 *
 * @param queue         Pointer to the queue where the item is to be added.
 * @param item          Pointer to the memory containing the item to be added.
 * @param block         Blocking behavior of the function:
 *                      - `EOS_BLOCK`: If the queue is full, the calling task will block until space becomes available.
 *                      - `EOS_NONBLOCK`: If the queue is full, the function will return immediately with `EOS_BLOCKED`.
 *
 * @return              Status of the operation:
 *                      - `EOS_OK`: Item was successfully added to the queue.
 *                      - `EOS_BLOCKED`: Queue is full, and blocking is disabled.
 *
 * @todo Improve the task unblocking method.
 */
EOS_status_t EOS_QueuePut(EOS_queue_id_t queue, const void *item, EOS_block_status_t block){
	EOS_EnterCritical();

	if (queue->count == queue->size)
	{
		if (block == EOS_BLOCK)
		{
			run_ptr->blocked = (void *)queue;
			EOS_ExitCritical();
			EOS_Suspend();
			EOS_EnterCritical();

			while (queue->count == queue->size)
			{
				EOS_ExitCritical();
				EOS_Suspend();
				EOS_EnterCritical();
			}
		}
		else
		{
			EOS_ExitCritical();
			return EOS_BLOCKED;
		}
	}

	void *dest = (uint8_t *)queue->buffer + (queue->tail * queue->item_size);
	memcpy(dest, item, queue->item_size);
	queue->tail = (queue->tail + 1) % queue->size;
	queue->count++;

	EOS_TaskUnblock(queue);

	EOS_ExitCritical();
	return EOS_OK;
}
