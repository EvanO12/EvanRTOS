# EvanRTOS
EvanRTOS is a lightweight, custom-built, real time operating system designed for STM32 Cortex-M microcontrollers. It is built around a priority-based, preemptive scheduler, and contains a variety of other features such as semaphores, queues, task delays and more. Inspired by the CMSIS v2 OS API/Wrapper, EvanRTOS contains a small subset of its operations and data structures, although with function names like EOS_Name instead of CMSIS OS' osName. 

EvanRTOS was developed with STM32's Hardware Abstraction Layer (HAL) in mind. While the os does not call any HAL functions, it does not explicitly set up Systick/PendSV interrupt priorities, and does not default the Systick Interrupt period to 1ms (which is done by default by HAL). For more info, see the **Getting Started** section. 

EvanRTOS relies on CMSIS library functions.

EvanRTOS has been confirmed to work on Cortex M7 and M4 processors, specifically, on the STM32H7 microcontroller. 

For more information, please see the rest of this document, the EvanRTOS kernel, and EvanRTOS demo project.

## DEMO: 
EvanRTOS_demo/CM7/Core/Src/eos.c contains an example of one may interact with EvanRTOS. Specifically, it contains 6 Tasks, 2 queues, and a semaphore, and uses other functionality such as EOS_Pause()/EOS_Resume() and EOS_Delay().

Here is an overview of what each Task Does:

- Task 0 acquires sem1, and increments t0count 10 times, at an interval of 500ms. It then releases the semaphore, and gets a value from queue1. It adds this value*10 to t0count, and sleeps for 1 second.

- Task 1 acquires sem1, and performs t1count += 1.00000423*t1count 5 times with a delay of 500 ms. It then releases the semaphore, and sleeps for 1 second. This showcases EvanRTOS's capability of perserving FPU Context during context switches. 

- Task 2 puts the value 4 into queue1, and then increments t2count, before sleeping for 1 second.

- Task 3 pauses Task 4, increments t3count ten times with a delay of 250 ms, before resuming Task 4 and sleeping for 5 seconds

- Task 4 increments t4count, and increments the static variable count, before putting count into queue2, and sleeping for 500 ms.

- Task 5 gets a value from queue 2, ands puts it in the t5_queue_item global variable before sleeping for 1 second.

Hopefully this overview shows what sort of behaviour EvanRTOS is capable of. Through its features, task synchronization, and data transfer are made easy. 
See the GIF below for a live view of global variables in this file to see how they change.

<div align="center">
  <img src="media/EvanRTOS_Demo.gif" alt="EvanRTOS Demo" width="852" height="480">
</div>

## RTOS Design
Below is an overview of how EvanRTOS is designed.

### Scheduler
##### Context Switching Mechanism
EvanRTOS uses the Systick and PendSV Interrupts in order to handle context switching between tasks. The Systick interrupt should be configured to run every 1ms by the user (or by HAL). 

- The Systick Interrupt Checks to see if the user defined task switching period has passed, and if it has, it triggers the PendSV Interrupt. The Systick interrupt also handles decrementing the timeout period for each task, set by EOS_Delay(time_delay) calls.
- The PendSV Interrupt saves register context of the prempted task onto its task stack, before calling the scheduler to determine the next task to run. After determining the next task, the scheduler pops its registers from the stack and branches to the task
- The PendSV Interrupt can also be triggered by queues/semaphores. If a task blocks on a semaphore or queue, it will trigger the interrupt. Additionally, if a task unblocks another task on a queue or semaphore, and that task is higher priority that the current running task, the PendSV interrupt will be triggered.

note: EvanRTOS uses a PSP/MSP design. Each task has its own stack, and own stack pointer. The PSP (process stack pointer) register is used for these stack pointers. Interrupts, such as PendSV, use the main stack pointer

##### EOS_Scheduler and Blocked States
 The EOS_Scheduler determines the next task, based off task priorities and task blocking. Essentially, the scheduler will choose the unblocked with the highest priority to run. 
 - If the current task fits these criteria, it will continue to run
 - Two or more tasks with the same, highest priority available priority, will take turns running (time splice) in a round-robin fashion

 Tasks can be in the following states:
- blocked: a task can be blocked on a semaphore/queue or from a EOS_Delay() (which sets a timeout).
- paused: a task is paused by another task. 
- running: a task is not blocked or paused, and is able to be chosen by the scheduler.

Pausing has precedence on blocking in EvanRTOS. What this means is a blocked task can also be paused. However, blocking around a paused task is a little less clear. A paused task, that is also blocked due to a timeout, will not continue to decrement the timeout. However, a paused task, that was previously blocked on either a semaphore, or a queue, can be unblocked. 
- *I am not sure if I like this behaviour yet. I may change it so that a paused task can not be unblocked until it is unpaused.*

### RTOS Features
##### Tasks
Each task in EvanRTOS has its own Task Control Block, and Task Stack. The Task Control block holds the task's stack pointer, and holds data on timeouts, paused/blocked states, and priority. It also holds a pointer to the next Task Control Block, in a circular linked list which holds all the TCBs. 

- When a new task is created, the user has the option of either statically or dynamically allocating stack space for it 
- TCBs are dynamically allocated

**Floating Point Tasks**
When a user creates a task, they must pass in a parameter to indicate if the task contains floating point operations. If it does, a floating point Task Stack is created, that contains more fields for the FPU registers. 

- Failure to identify tasks as floating point tasks will lead to errors in the task running.

**The Idle Task**
EvanRTOS contains an Idle Task. The Idle Task has a lower priority than any user created task, and never blocks. This means that the Idle Task is always able to run, and will run if all the other tasks are blocked. 

##### Semaphores
EvanRTOS uses counting semaphores for its semaphore logic. There are three supported semaphore functions:

- EOS_SemaphoreNew(): create a new semaphore
- EOS_SemaphoreAcquire(): take/acquire a semaphore -> can only be called from task context.
- EOS_SemaphoreRelease(): release a semaphore
 
A semaphore is represented by EOS_semaphore_t. Each semaphore is dynamically allocated, during startup. For users of EvanRTOS, semaphores are identifed by ids with EOS_semaphore_id_t, created by EOS_SemaphoreNew(). Under the hood, this id datatype is just a pointer to the semaphore. However, users of EvanRTOS can ignore this, and simply use the
id by passing it into EOS_SemaphoreAcquire() and EOS_SemaphoreRelease() calls.

Each semaphore is created with a max count, which is a uint8_t number. The semaphore starts with this count. The count will decrement when acquired until it equals 0. Tasks trying to acquire a semaphore with a count of 0 will block, and will not run until a task increments the
semaphore and unblocks them.

When a task releases a semaphore, the count will increment. It will then look to unblock any tasks waiting on said semaphore.
 
##### Queues
EvanRTOS uses a FIFO based queue implementation. The queue is of fixed size, and will act as a circular buffer in the sense that
head and tail pointers wrap around when the exceed the length of the queue. The tail pointer points to the position of where
the next item is to be added, while the head points to next item to be removed from the queue.

EvanRTOS queues currently support the following operations:
- EOS_QueueCreate();
- EOS_QueueGet();
- EOS_QueuePut();
 
EOS_QueueGet and EOS_QueuePut can be called from interrupt contexts by adding EOS_block_status_t EOS_NO_BLOCK as the third parameter.
This will return EOS_BLOCKED if the queue is full, and a message cannot be added, or if the queue is empty, and a message cannot be
retrieved. Otherwise, EOS_OK will be returned on success of the operation.

By using EOS_block_status_t EOS_BLOCK,the put and get functions can be called from tasks. The tasks will attempt the operation, and enter a blocked state if they cannot yet be executed.

Queues are represented by the EOS_queue_t datatype. Like semaphores, when a queue is created, a id is returned. This is the EOS_queue_id_t type which is a pointer to the queue under the hood. For users of EvanRTOS, this can be simply treated as an id to be passed into the get and put functions.

All queues in EvanRTOS are dynamically allocated.

##### Timed Task Sleeping  (EOS_Delay())
Tasks in EvanRTOS can enter a blocked state for a set period of time by using EOS_Delay().
- EOS_Delay() must be called by the Task going to sleep
- EOS_Delay() must be passed a timeout period in ms as a parameter

When EOS_Delay() is called, the calling task will enter a blocked state, stopping on the EOS_Delay() call. It will also load the timeout period into its task control block. The Systick interrupt will decrement the timeout in the TCB everytime it runs, until it reaches zero, at which point the task is unblocked, and eligible to the scheduler.

##### EOS_Pause() and EOS_Resume()
As mentioned, there is a separate paused state that can be applied to tasks. 
- A task can pause itself, be paused by another task, or be paused by an interrupt
- When in the paused state, a task will be ineligible to the scheduler, until unpaused

##### Critical Sections
Critical Sections in the EOS kernel, and Queue/Semaphore Implementations are protected by disabling interrupts. A pretty liberal application of critical sections was given in EvanRTOS, so staying on the side of caution, interrupts are quite commonly disabled. 

- This guarantees atomicity as EvanRTOS is only a single core RTOS (currently).


## Using EvanRTOS

### Getting Started
In order to use EvanRTOS, you first want to clone this repo or download the files in EvanRTOS_kernel. You should then add these files to your project. 

In order to ensure EvanRTOS works, there are some things to do:
1. Ensure that the Systick Interrupt is set to run every 1 ms
2. Set the priority of PendSV interrupt to be the lowest, and the priority of the Systick Interrupt to be the second lowest.
3. If you are using HAL, and STM32CUBEIDE's code generation, you want to disable code generation for the Systick and PendSV interrupt. This is because these are defined in eos_kernel.c

With this setup, as long as you are on a Cortex-M microcontroller, with the CMSIS Library available, EvanRTOS *should* work. I have yet to test it in a project outside of a STM32 HAL environment, but will soon. 


### Interacting with EvanRTOS
eos.c and the eos_kernel function specifications provide the best insight into how to use EvanRTOS. However, here is a brief overview.

##### Initializing EvanRTOS
In order to start up EvanRTOS, you want to call 
```c
EOS_Init(DEFAULT_TASK_PERIOD);
```

However, you want to make sure that all of your tasks are setup before you do so. I recommend setting up your own EvanRTOS_Init() function, like the one that can be seen in eos.c in the demo. You want to call this function from your main function, before the forever loop. This function should  then setup your queues, semaphores, and tasks, before calling EOS_Init(). For example:

```c
int main(){
    GPIO_Init();
    HAL_INIT();
    ... //just examples
    EvanRTOS_Init();

}
...

void EvanRTOS_Init(){

	//init queues, semaphores, and tasks...

	EOS_Init(DEFAULT_TASK_PERIOD); //scheduler preempts every 1 ms

}
```
This will pass full control to EvanRTOS, which will begin scheduling tasks.

##### Creaing a Task
We can create a task with a statically allocated stack, that uses the FPU as follows:
```c 
int32_t stack[128];
EOS_task_id_t task_handle;

void EvanRTOS_Init(){
    task_handle = EOS_ThreadNew(function_ptr, PRIORITY_LOW, &stack, stack_size, EOS_USE_FPU);
}
``` 
If we instead wanted to create a task with a dynamically allocated stack, that does not use the FPU, we can do the following:
```c
void EvanRTOS_Init(){
    task_handle = EOS_ThreadNew(function_ptr, PRIORITY_HIGH, NULL, stack_size, EOS_NO_FPU);
}
```

##### Semaphores
We can create a semaphore by doing the following:
```c 
#define SEM_1_COUNT 3 //you choose
EOS_semaphore_id_t semaphore;

EvanRTOS_Init(){
    semaphore = EOS_SemaphoreNew(SEM_1_COUNT);
}
```
To then acquire and release the semaphore we then have to call:

```c
void task(){
    while(1){
        EOS_SemaphoreAcquire(semaphore);
        // do something
        EOS_SemaphoreRelease(semaphore);
        //do something
}}
```
**Note** that you want to avoid calling EOS_SemaphoreAcquire after EOS_SemaphoreRelease without operations or a delay between, as you pose the risk of one task (equal or higher priority to others) hogging the semaphore, as the EOS_SemaphoreRelease() will only trigger the scheduler, if the task it unblocks has higher priority than the current running task.


##### Queues
Working with queues is similar. We can create a queue with:
```c
EOS_queue_id_t queue;
EvanRTOS_Init(){
   queue = EOS_QueueCreate(NUM_ELEMENTS, ELEMENT_SIZE);
}
```
The put and get operations on the queue look like the following:
```c

void InterruptHandler(){
    int value = 8;
    EOS_QueuePut(queue, &value, EOS_NO_BLOCK);
}

void TaskA(){
    while(1){
        int value = 8;
        EOS_QueuePut(queue, &value, EOS_BLOCK);
        //do something else
    }
}

void TaskB(){
    while(1){
        int value = 0;
        EOS_QueueGet(queue, &value, EOS_BLOCK);
        ProcessValue(value);
    }
}
```
##### EvanRTOS MISC
Some other operations on EvanRTOS you may want to do:
```c
void task(){
	while(1){
        EOS_Pause(task2_handle);
	    tcount++;
		EOS_Delay(1000); //will block the task for 1 second
        EOS_Resume(task2_handle);
	}
}
void task2(){
    while(1){
        //...
    }
}
```


## TODOs
1. Improve Algorithms for keeping track of blocked tasks, and blocking tasks
    - iterating through a linked list is probably not the fastest way of doing this, so I look for better ways
2. Support more of the CMSIS RTOS API's functions
3. Provide more user control of semaphores, queues, and tasks with attribute structs that can be passed as arguments on creation.
4. More extensively test EvanRTOS, on a variety of targets, with a greater variety of program.


## Acknowledgements
- EvanRTOS was inspired by FreeRTOS, and other lightweight RTOS implementations.
    - the PendSV interrupt is very similar to, and inspired by the FreeRTOS implementation in their kernel: https://github.com/FreeRTOS/FreeRTOS-Kernel

Other Resources:
- Real-Time Bluetooth Networks Course on edX by UT Austin: 
https://www.edx.org/learn/computer-programming/the-university-of-texas-at-austin-real-time-bluetooth-networks-shape-the-world
    - Learned a lot about how an RTOS works from this course.
- ARM Cortex-M RTOS Context Switching, by Chris Coleman: https://interrupt.memfault.com/blog/cortex-m-rtos-context-switching#arm-cortex-m-rtos-context-switching
    - Very informative Article on Context Switching on Cortex-M processors
