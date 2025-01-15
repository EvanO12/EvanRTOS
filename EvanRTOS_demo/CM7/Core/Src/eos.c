/*
 * eos.c
 *
 *      eos.c is not part of the core EvanRTOS functionality. Instead, it is an example of how users may interact with EvanRTOS. eos.c can contain
 *      the task code for each task the user wants to implement. It should also contain some sort of initalization function, where tasks, queues, and semaphores are created.
 *      When using EvanRTOS, this file is a good place to start to learn how it works.
 *
 *      Right now, this file has a number of different tasks, each showcasing different functionality of EvanRTOS. If you are using
 *      STM32CUBEIDE, you can load the names of the global variables into the live expressions tool, and see how they change as the
 *      RTOS runs.
 */


/*	INCLUDES	*/
//#include "eos.h"
#include "eos_kernel.h"
#include "eos_semaphore.h"
#include "eos_queue.h"

/* DEFINES	*/


/*	SEMAPHORES	*/
EOS_semaphore_id_t sem1;

/*	QUEUES	*/
EOS_queue_id_t queue1;
EOS_queue_id_t queue2;

/*	TASKS	*/

/*Task 0*/
EOS_task_id_t task0_handle;
EOS_priority_t task0_priority = PRIORITY_HIGH;
void task0(void);
int32_t task0_stack[128]; //this task has a statically allocated stack

/*Task 1*/
EOS_task_id_t task1_handle;
EOS_priority_t task1_priority = PRIORITY_MEDIUM;
void task1(void);
uint32_t task1_stack_size = 128; //this task's stack will be dynamically allocated (64 words)

/*Task 2*/
EOS_task_id_t task2_handle;
EOS_priority_t task2_priority = PRIORITY_LOW;
void task2(void);
uint32_t task2_stack_size = 128; //dynamically allocated task stack

/*Task 3 */
EOS_task_id_t task3_handle;
EOS_priority_t task3_priority = PRIORITY_MEDIUM;
void task3(void);
uint32_t task3_stack_size = 128;

/*Task 4*/
EOS_task_id_t task4_handle;
EOS_priority_t task4_priority = PRIORITY_MEDIUM;
void task4(void);
uint32_t task4_stack_size = 128;


/*Task 5*/
EOS_task_id_t task5_handle;
EOS_priority_t task5_priority = PRIORITY_LOW;
void task5(void);
uint32_t task5_stack_size = 128;

/*	USER VARIABLES	*/

uint32_t t0count = 0;
double t1count = 1;
uint32_t t2count = 0;
uint32_t t3count = 0;
uint32_t t4count = 0;

uint32_t t5_queue_item = 0;


/*	USER CODE	*/



/* Function to initalize the user tasks, queues, and semaphores.
 *
 * The user should create this function, and call it before the forever loop in the main function.
 *
 * This funtion never returns, as it passes control over to EvanRTOS.
 *
 */
void EvanRTOS_Init(){

	sem1 = EOS_SemaphoreNew(1);


	queue1 = EOS_QueueCreate(16, sizeof(uint8_t)); //create a queue that holds 16 byte-sized objects
	queue2 = EOS_QueueCreate(8, sizeof(uint32_t)); //creates a queue that holds 16 word-sized objects

	task0_handle = EOS_ThreadNew(task0, task0_priority, task0_stack, 128, EOS_NO_FPU); 				//static task stack allocation
	task1_handle = EOS_ThreadNew(task1, task1_priority, NULL, task1_stack_size, EOS_USE_FPU); 		   //dynamic task stack allocation
	task2_handle = EOS_ThreadNew(task2, task2_priority, NULL, task2_stack_size, EOS_NO_FPU);
	task3_handle = EOS_ThreadNew(task3, task3_priority, NULL, task3_stack_size, EOS_NO_FPU);
	task4_handle = EOS_ThreadNew(task4, task4_priority, NULL, task4_stack_size, EOS_NO_FPU);
	task5_handle = EOS_ThreadNew(task5, task5_priority, NULL, task5_stack_size, EOS_NO_FPU);

	EOS_Init(DEFAULT_TASK_PERIOD); //scheduler preempts every 1 ms


}

void task0(){

	while(1)
	{

		EOS_SemaphoreAcquire(sem1);
		for (int i = 0; i < 10; i++)
		{
			t0count++;
			EOS_Delay(500);
		}

		EOS_SemaphoreRelease(sem1);

		int value = 0;
		EOS_QueueGet(queue1, &value, EOS_BLOCK);
		t0count += value*10;
		EOS_Delay(1000);
	}

}


void task1(){

	while(1)
	{
		EOS_SemaphoreAcquire(sem1);
		for(int i =0; i < 5; i++)
		{
			t1count += 1.00000423*t1count;
			EOS_Delay(500);
		}

		EOS_SemaphoreRelease(sem1);
		EOS_Delay(1000);
	}
}

void task2(){

	while(1){
		uint8_t value = 4;
		EOS_QueuePut(queue1, &value, EOS_BLOCK);
		t2count++;
		EOS_Delay(1000);
	}
}

void task3(){

	while(1){
		EOS_Pause(task4_handle);

		for(int i = 0; i < 10; i++)
		{
			t3count++;
			EOS_Delay(250);
		}
		EOS_Resume(task4_handle);
		EOS_Delay(5000);
	}
}

void task4(){

	while(1)
	{
		static uint32_t count = 0;
		t4count++;
		count++;
		EOS_QueuePut(queue2, &count, EOS_BLOCK);
		EOS_Delay(500);

	}
}


void task5(){
	while(1)
	{
		uint32_t recent = 0;
		EOS_QueueGet(queue2, &recent, EOS_BLOCK);
		t5_queue_item = recent;
		EOS_Delay(1000);
	}
}

