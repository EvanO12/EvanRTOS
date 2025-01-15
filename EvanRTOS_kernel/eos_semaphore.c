/*
 * eos_semaphore.c
 *
 *      EvanRTOS uses counting semaphores for its semaphore logic. There are three supported semaphore functions:
 *
 *      	EOS_SemaphoreNew(): create a new semaphore
 *      	EOS_SemaphoreAcquire(): take/acquire a semaphore -> can only be called from task context.
 *      	EOS_SemaphoreRelease(): release a semaphore
 *
 *
 *
 *      A semaphore is represented by EOS_semaphore_t. Each semaphore is dynamically allocated, during startup. For users
 *      of EvanRTOS, semaphores are identifed by ids with EOS_semaphore_id_t, created by EOS_SemaphoreNew().
 *      Under the hood, this id datatype is just a pointer to the semaphore. However, users of EvanRTOS can ignore this, and simply use the
 *      id by passing it into EOS_SemaphoreAcquire() and EOS_SemaphoreRelease() calls.
 *
 *      Each semaphore is created with a max count, which is a uint8_t number. The semaphore starts with this count. The count will decrement when
 *      taken until it equals 0. Tasks trying to acquire a semaphore with a count of 0 will block, and will not run until a task increments the
 *      semaphore and unblocks them.
 *      When a task releases a semaphore, the count will increment. It will then look to unblock any tasks waiting on said semaphore.
 *
 *
 */

/*	INCLUDES	*/
#include "eos_semaphore.h"


/*	SEMAPHORE FUNCTIONALITY		*/


/**
 * @brief Acquires a semaphore, blocking the current task if the semaphore is not free (count is 0)
 *
 * @param semaphore The semaphore to acquire.
 * @return EOS_OK on successful acquisition.
 */
EOS_status_t EOS_SemaphoreAcquire(EOS_semaphore_id_t semaphore) {
    EOS_EnterCritical();

    while (1) {
        if (semaphore->count > 0) {
            semaphore->count--;
            EOS_ExitCritical();
            return EOS_OK;
        } else {
            run_ptr->blocked = semaphore;
            EOS_ExitCritical();
            EOS_Suspend();
            EOS_EnterCritical();
        }
    }
}



/**
 * @brief Releases semaphore, and unblocking task waiting on it.
 *
 * @param semaphore The semaphore to release.
 * @return EOS_OK on success, EOS_ERROR if the semaphore is already at its maximum count.
 */
EOS_status_t EOS_SemaphoreRelease(EOS_semaphore_id_t semaphore) {
    EOS_EnterCritical();

    //protect against spamming of EOS_Semaphore Release
    if (semaphore->count >= semaphore->max_count)
    {
          EOS_ExitCritical();
          return EOS_ERROR;
      }

    semaphore->count++;

    EOS_TaskUnblock(semaphore);

    EOS_ExitCritical();
    return EOS_OK;
}


/**
 * @brief Creates a new semaphore with the specified count
 *
 * @param count The initial and maximum count of the semaphore.
 * @return A pointer to the newly created semaphore, or NULL if memory allocation fails.
 *
 * @note All EvanRTOS Semaphores are dynamically allocated.
 */
EOS_semaphore_id_t EOS_SemaphoreNew(uint8_t count){
	EOS_semaphore_t* semaphore = (EOS_semaphore_t *)malloc(sizeof(EOS_semaphore_t));

	if (semaphore == NULL)
	{
		return NULL;
	}

	semaphore->count = count;
	semaphore->max_count = count;

	return semaphore;
}
