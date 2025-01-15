/*
 * eos.h
 *
 *  Created on: Jan 10, 2025
 *      Author: EO12
 */

#ifndef INC_EOS_H_
#define INC_EOS_H_



/*	INCLUDES	*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "main.h"
//#include "eos_semaphore.h"
//#include "eos_queue.h"
//#include  "eos_dual_core.h"

/*		ENUMERATIONS		*/
typedef enum {
	EOS_ERROR = 0,
	EOS_OK = 1,
	EOS_BLOCKED = 2,
	EOS_USE_FPU = 3,
	EOS_NO_FPU = 4
} EOS_status_t;

typedef enum{
	EOS_BLOCK = 1,
	EOS_NO_BLOCK = 0
} EOS_block_status_t;

typedef enum{
	PRIORITY_IDLE = 0,
	PRIORITY_LOW = 1,
	PRIORITY_MEDIUM = 2,
	PRIORITY_HIGH = 3
}EOS_priority_t;

/*		CONSTANTS		*/
#define EOS_TIMED_OUT ((void*)2)
#define EOS_PAUSED 1
#define DEFAULT_TASK_PERIOD 1


/*		CUSTOM DATATYPES		*/
typedef struct eos_TCB_t {
 int32_t *sp;
 void* blocked;
 struct eos_TCB_t *next;
 uint32_t timeOut;
 uint8_t priority;
 uint8_t paused;
} EOS_TCB_t;

typedef EOS_TCB_t* EOS_task_id_t;

extern EOS_TCB_t* run_ptr;



/*		FUNCTION PROTOTYPES		*/
EOS_task_id_t EOS_ThreadNew(void* function, EOS_priority_t priority, int32_t* task_stack, uint32_t stack_size, EOS_status_t use_fpu);
void EOS_Init(uint32_t user_task_period);

void PendSV_Handler(void);
void SysTick_Handler(void);

void EOS_Delay(uint32_t timeout);
EOS_status_t EOS_Pause(EOS_task_id_t task);
EOS_status_t EOS_Resume(EOS_task_id_t task);

void EOS_Suspend();
void EOS_EnterCritical();
void EOS_ExitCritical();
void EOS_TaskUnblock(void* item);

#endif /* INC_EOS_H_ */
