/*
 * eos_kernel.c
 *
 *      Basic Functions for EvanRTOS, a basic, lightweight RTOS, custom built for the STM32h747 MCU. EvanRTOS contains functions inspired by the
 *      CMSIS RTOSv2 API.
 *
 *      EvanRTOS operates as a single core, pre-emptive scheduler. Tasks can be given a priority of EOS_priority_t,
 *      and will run based off these priorities. If a high priority task runs, and never blocks, a lower priority task will never be given cpu time.
 *      Two tasks of the same priority will run equally. If a high priority task becomes unblocked while a low priority task is still running,
 *      the high priority task will preempt the lower priority task in order to run.
 *
 *      In EvanRTOS, each task has a stack, that can be either dynamically or statically allocated. Passing NULL into the third field of EOS_ThreadNew()
 *      will result in the kernel dynamically allocating space for that task, with the size in the fourth field of the function. Otherwise, the user
 *      must create stack space and pass it into the function.
 *
 *
 *      For a more thorough overview, please look at README.MD.
 */


#include <stdint.h>
#include <stdlib.h>
#include "eos_kernel.h"
#include "main.h"
#include <string.h>



/*	LOCAL FUNCTION PROTOTYPES	*/
static void EOS_scheduler(void);
static void EOS_HandleTimeout();
static void EOS_InitStack(int32_t* task_stack, uint32_t stack_size, void* function);
static void EOS_InitFpuStack(int32_t* task_stack, uint32_t stack_size, void* function);
static __attribute__((naked))void EOS_Start();
static void idleTask();


/*	GLOBAL VARIABLES*/
uint32_t task_period = DEFAULT_TASK_PERIOD;
uint8_t scheduler_enable = 0;

int32_t idle_stack[32];
EOS_TCB_t idle_task = {
		.blocked = 0,
		.next = &idle_task,
		.priority = PRIORITY_IDLE,
		.sp = &idle_stack[15],
		.timeOut = 0,
		.paused = 0
};

EOS_TCB_t* run_ptr = &idle_task;


/*		EOS STARTUP		*/


/**
 * @brief Creates a new thread and adds it to the list of tasks.
 *
 * @details The task control block is always dynamically allocated. Stack space for the stack can either be
 * 			statically or dynamically allocated.
 *          If the provided task stack is NULL, it allocates memory for the stack dynamically. Otherwise,
 *          the user must provide statically declared task space.
 *
 * @param function Pointer to the function of the new task.
 * @param priority Priority level of the task. Must not exceed `PRIORITY_HIGH`.
 * @param task_stack Pointer to the memory allocated for the task stack. If `NULL`, the stack will
 *                   be allocated dynamically.
 * @param stack_size Size of the stack in words. Must be at least 64 words.
 * @param use_fpu Status indicating whether the task uses floating point operations.  EOS_USE_FPU implies the task contains
 * 		  	floating point operations. EOS_NO_FPU is the default, when the task contains no floating point operations.
 * 			This must be defined by the user.
 *
 * @return A pointer to the newly created task's TCB (as `EOS_task_id_t`), or `EOS_ERROR` on failure.
 *
 */
EOS_task_id_t EOS_ThreadNew(void* function, EOS_priority_t priority, int32_t* task_stack, uint32_t stack_size, EOS_status_t use_fpu){

	if (stack_size < 64)
	{
		return EOS_ERROR;
	}

	if (priority > PRIORITY_HIGH)
	{
		return EOS_ERROR;
	}
	EOS_TCB_t* control_block = (EOS_TCB_t*)malloc(sizeof(EOS_TCB_t));

	int32_t* sp;

	if (task_stack == NULL)
	{
		sp = (int32_t*)malloc(stack_size * sizeof(int32_t));

		if(sp == NULL)
		{
			free(control_block);
			return EOS_ERROR;
		}


		if(use_fpu == EOS_USE_FPU)
		{
			EOS_InitFpuStack(sp, stack_size, function);
			sp = &sp[stack_size - 51];
		}
		else
		{
			EOS_InitStack(sp, stack_size, function);
			sp = &sp[stack_size - 17];
		}


	}
	else
	{
		if(use_fpu == EOS_USE_FPU)
		{
			EOS_InitFpuStack(task_stack, stack_size, function);
			sp = &task_stack[stack_size - 51];
		}
		else
		{
			EOS_InitStack(task_stack, stack_size, function);
			sp = &task_stack[stack_size - 17];
		}

	}

	if (control_block == NULL){
		free(sp);
		return EOS_ERROR;
	}

	control_block->sp = sp;
	control_block->priority = priority;
	control_block->blocked = 0;
	control_block->timeOut = 0;
	control_block->paused = 0;

	EOS_TCB_t* temp = run_ptr;
	while (temp->next != run_ptr)
	{
		temp = temp->next;
	}

	temp->next = control_block;
	control_block->next = run_ptr;


	return control_block;
}


/**
 * @brief Initializes the RTOS and starts the scheduler.
 *
 * @param user_task_period The desired task period (in ms) specified by the user. Default is 1ms.
 *
 */
void EOS_Init(uint32_t user_task_period){
	EOS_EnterCritical();
	scheduler_enable = 1;

	EOS_InitStack(idle_stack, 32, idleTask);


	if (user_task_period != task_period){
		task_period = user_task_period;
	}

	__set_CONTROL(__get_CONTROL() | CONTROL_SPSEL_Msk); //enables PSP mode


	EOS_Start();
}


/**
 * @brief Starts the first task by restoring its context and enabling execution.
 *
 * @details This function is called to launch the RTOS scheduler. It will run the task currently pointed to by run_ptr.
 *
 * @return None. This function does not return to the caller.
 */

static __attribute__((naked))void EOS_Start(){

	__asm volatile(
			"LDR R0, =run_ptr      \n"
			"LDR R1, [R0]          \n"
			"LDR SP, [R1]          \n"
			"ADD SP, SP, #4		   \n"
			"POP {R4-R11}          \n"
			"POP {R0-R3}           \n"
			"POP {R12}             \n"
			"ADD SP, SP, #4        \n"
			"POP {LR}              \n"
			"ADD SP, SP, #4        \n"
			"CPSIE I               \n"
			"BX LR                 \n"
		);
}




/*		EOS CORE IMPLEMENTATION (CONTEXT SWITCHING)		*/


/**
 * @brief Priority-Based Round Robin Scheduler.
 *
 * The highest, unblocked task will run. Two tasks of equal priority running will timesplice, so that they both get
 * equal running time. If there are no unblocked tasks, the idle task will run.
 */
static void EOS_scheduler(void)
{

	EOS_TCB_t* current_ptr = run_ptr->next;
	EOS_TCB_t* start_ptr = run_ptr;
    if (run_ptr->blocked != 0 || run_ptr->paused != 0)
    {
    	start_ptr = &idle_task;
    	current_ptr = start_ptr->next;
    }

    EOS_TCB_t* best_pointer = start_ptr;
	while (current_ptr != run_ptr){
		if (current_ptr->blocked == 0 && current_ptr->paused == 0 && current_ptr->priority >= best_pointer->priority){
			best_pointer = current_ptr;
		}
		current_ptr = current_ptr->next;
	}
	run_ptr = best_pointer;

	return;
}


/**
 * @brief PendSV exception handler for context switching.
 *
 * This function draws inspiration from the FreeRTOS Kernel PendSV_Handler, and shares some similarities.
 * Credit here:	https://github.com/FreeRTOS/FreeRTOS-Kernel.
 */
__attribute__((naked))void PendSV_Handler(void)
{
	 __asm volatile (
	        "CPSID I\n"
	        "MRS R2, PSP\n"
	        "TST LR, #0x10\n" //check if in fpu mode
	        "IT EQ\n"
	        "VSTMDBEQ R2!, {S16-S31}\n" //save fpu registers
			"STMDB R2!, {R4-R11}\n"
			"STMDB R2!, {R14}\n"
	        "LDR R0, =run_ptr\n"
	        "LDR R1, [R0]\n"
	        "STR R2, [R1]\n"
	        "STMDB SP!, {R0}\n"
	        "BL EOS_scheduler\n"
	        "LDMIA SP!, {R0}\n"
	        "LDR R1, [R0]\n"
	        "LDR R2, [R1]\n"
			"LDMIA R2!, {R14}\n"
			"LDMIA R2!, {R4-R11}\n"
	        "TST LR, #0x10\n"
	        "IT EQ\n"
	        "VLDMIAEQ R2!, {S16-S31}\n"
	        "MSR PSP, R2\n"
	        "CPSIE I\n"
	        "BX LR\n"
	        ".align 4\n"
	    );
}


/**
 * @brief SysTick interrupt handler.
 *
 * Enables the PendSV interrupt when it is time to perform a context switch. Handles task timeout decrementing.
 *
 * @note HAL initalizes the systick to run at 1 ms intervals based off the clock speed of the processor.
 * If you are not using HAL, please do this yourself otherwise certain EvanRTOS features (ie EOS_Delay()) will not work.
 */
void SysTick_Handler(void)
{
	EOS_EnterCritical();
	static uint32_t eos_tickCounter = 0;
	HAL_IncTick();

	eos_tickCounter++;

	if (eos_tickCounter >= task_period && scheduler_enable == 1)
	{
		eos_tickCounter = 0;
		EOS_HandleTimeout();
  		SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
  	}

	EOS_ExitCritical();

}



/*		BASIC RTOS OPERATIONS		*/


/**
 * @brief Suspends the currently running task for a specified timeout period.
 *
 * @param timeout The delay period in ms.
 *                If the period is 0, the timeout will be 1ms.
 *
 * @return None.
 *
 */
void EOS_Delay(uint32_t timeout){
	EOS_EnterCritical();
	if (timeout == 0){
		timeout = 1;
	}
	run_ptr->blocked = EOS_TIMED_OUT;
	run_ptr->timeOut = timeout;
	EOS_ExitCritical();
	EOS_Suspend();

}


/**
 * @brief Pauses the specified task, blocking it indefinitely.
 *
 * @param task ID of the task being paused.
 *
 * @return EOS_OK if the task was successfully paused.
 *         EOS_ERROR if the task pointer is NULL.
 *
 * This function can be called from an interrupt, the task being paused, or another task.
 */
EOS_status_t EOS_Pause(EOS_task_id_t task)
{
	EOS_EnterCritical();

	if (task == NULL)
	{
		return EOS_ERROR;
	}

	if(task->paused != 0)
	{
		return EOS_ERROR;
	}

	task->paused = EOS_PAUSED;

	if (task == run_ptr)
	{
		EOS_ExitCritical();
		EOS_Suspend();
	}

	EOS_ExitCritical();
	return EOS_OK;
}


/**
 * @brief Resumes a task that was previously paused using EOS_Pause.
 *
 * @param task ID of the task being resumed.
 *
 * @return EOS_OK if the task was successfully resumed.
 *         EOS_ERROR if the task pointer is NULL or the task is not in a paused state.
 *
 * @note A task must be resumed by another task or an interrupt.
 * 		 The paused state is different from the blocked state.
 */
EOS_status_t EOS_Resume(EOS_task_id_t task)
{
	EOS_EnterCritical();

	if (task == NULL)
	{
		return EOS_ERROR;
	}

	if (task->paused != EOS_PAUSED)
	{
		return EOS_ERROR;
	}

	task->paused = 0;
	EOS_ExitCritical();
	return EOS_OK;
}



/*		HELPER FUNCTIONS		*/


/**
 * @brief Triggers the PendSV interrupt to handle context switching
 */
void EOS_Suspend(){
	SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}


/**
 * @brief Disables interrupts to allow for critical sections to be run without worry of a context switch.
 */
void EOS_EnterCritical(){
	__disable_irq();
}


/**
 * @brief Enables interrupts after critical section code has finished running.
 */
void EOS_ExitCritical(){
	__enable_irq();
}



/**
 * @brief Unblocks the highest-priority task waiting on the specified resource (queue or semaphore).
 * 			If the priority of the task is higher than the current running task, it calls the scheduler.
 *
 * @param item Pointer to the resource (queue or semaphore) on which tasks may be blocked.
 *             The function uses a `void*` to allow handling of multiple types of synchronization primitives.
 */
void EOS_TaskUnblock(void* item){
    EOS_TCB_t* tmp_ptr = run_ptr->next;
    EOS_TCB_t* start_ptr = run_ptr;
    EOS_TCB_t* best_ptr = NULL;

    do
    {
    	if (tmp_ptr->blocked == item)
    	{
            if (best_ptr == NULL || tmp_ptr->priority > best_ptr->priority)
            {
                best_ptr = tmp_ptr;
            }
        }

        tmp_ptr = tmp_ptr->next;

    } while (tmp_ptr != start_ptr);

    if (best_ptr != NULL)
    {
        best_ptr->blocked = 0;

        if (best_ptr->priority > run_ptr->priority)
        {
            EOS_ExitCritical();
            EOS_Suspend();
        }
    }
}

/**
 * @brief Handles task timeouts and unblocks tasks whose timeouts have expired.
 */
static void EOS_HandleTimeout(){
	EOS_TCB_t* head = run_ptr;
	EOS_TCB_t* current = run_ptr->next;

	 while (current != head)
	 {
		 if (current->blocked == EOS_TIMED_OUT && current->paused == 0)
		 {
			 if (current->timeOut > 0)
			 {
				 current->timeOut--;

				 if (current->timeOut == 0)
				 {
					 current->blocked = 0;
				 }
			 }
		 }
		  current = current->next;
	  }
}


/**
 * @brief Initializes the stack for a new task, that does not use the FPU.
 *
 * @param task_stack Pointer to the base of the task's stack memory
 * @param stack_size Size of the stack in 32-bit words.
 * @param function   Pointer to the task's function.
 *
 */
static void EOS_InitStack(int32_t* task_stack, uint32_t stack_size, void* function){

	task_stack[stack_size-1] = 0x01000000;  //xpsr
	task_stack[stack_size-2] = (int32_t)function; //pc
	task_stack[stack_size-3] = 0xFFFFFFFD; 	//lr
	task_stack[stack_size-4] = 0xDEADBEEA;  //r12
	task_stack[stack_size-5] = 0xDEADBEEB;  //r3
	task_stack[stack_size-6] = 0xDEADBEEC;  //r2
	task_stack[stack_size-7] = 0xDEADBEED;	//r1
	task_stack[stack_size-8] = 0xDEADBEEF;	//r0
	task_stack[stack_size-9] = 0xDEADBEEF;	//r11
	task_stack[stack_size-10] = 0xDEADBEAA;	//r10
	task_stack[stack_size-11] = 0xDEADBEDF; //r9
	task_stack[stack_size-12] = 0xDEADBEBF; //r8
	task_stack[stack_size-13] = 0xDEADBECF; //r7
	task_stack[stack_size-14] = 0xDEADBECC; //r6
	task_stack[stack_size-15] = 0xDEADBEDD; //r5
	task_stack[stack_size-16] = 0xDEADBAAA; //r4
	task_stack[stack_size-17] = 0xFFFFFFFD; //store LR in fixed place for ease of context switching


}


/**
 * @brief Initializes the stack for a new task, that uses the FPU.
 *
 * @param task_stack Pointer to the base of the task's stack memory
 * @param stack_size Size of the stack in 32-bit words.
 * @param function   Pointer to the task's function.
 *
 */
static void EOS_InitFpuStack(int32_t* task_stack, uint32_t stack_size, void* function){
	task_stack[stack_size-1] = 0xDEADBEEF;
	task_stack[stack_size-2] = 0x00000000; //fpscr

	for(int i = 3; i < 19; i++)
	{
		task_stack[stack_size-i] = 0x00000000; //S15->S0 fpu registers
	}

	task_stack[stack_size-19] = 0x01000000; //xPSR
	task_stack[stack_size-20] = (int32_t)function;
	task_stack[stack_size-21] = 0xFFFFFFED; //lr
	task_stack[stack_size-22] = 0xDEADBEEA;  //r12
	task_stack[stack_size-23] = 0xDEADBEEB;  //r3
	task_stack[stack_size-24] = 0xDEADBEEC;  //r2
	task_stack[stack_size-25] = 0xDEADBEED;	//r1
	task_stack[stack_size-26] = 0xDEADBEEF;	//r0

	for (int i = 27; i < 43; i++)
	{
		task_stack[stack_size - i] = 0x00000000; //S31-s16 fpu registers
	}
	for(int i = 43; i < 51; i++)
	{
		task_stack[stack_size-i] = 0xDEADBEEF; //r11-r4
	}

	task_stack[stack_size-51] = 0xFFFFFFED; //store LR in fixed place for ease of context switching
}


/*	IDLE TASK	*/
void idleTask(){

	while(1){

	}
}
