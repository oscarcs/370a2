/**
 * File:    dispatchQueue.c
 * Author:  osim082
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "dispatchQueue.h"

#define NUM_CORES sysconf(_SC_NPROCESSORS_ONLN)

void _dispatcher_concurrent(dispatch_queue_t*);
void _dispatcher_serial(dispatch_queue_t*);
void _queue_add_task(dispatch_queue_t*, task_t*);
task_t* _queue_get_task(dispatch_queue_t*);

/**
 * The main dispatch function run by the control thread of this queue.
 * We 
 */
void _dispatcher_concurrent(dispatch_queue_t* queue) {
    
    while (1) {

        // Wait for a task to arrive
        sem_wait(&(queue->sem_new_task));

        printf("New task recieved!\n");

        // Get the task
        task_t* task = _queue_get_task(queue);

        // Schedule the task to an available thread
        

        // If the task has a semaphore, post to it
        if (task->type == SYNC) {
            sem_post(&(task->sem_task));
        }

        // Destroy the task

    }

    

}

/**
 * Function to run the tasks serially
 */
void _dispatcher_serial(dispatch_queue_t* queue) {
    while (1) {
        // Wait for a new task:
        sem_wait(&(queue->sem_new_task));


    }
}

/**
 * Helper function to append a task to a queue.
 */
void _queue_add_task(dispatch_queue_t* queue, task_t* task) {

    // We need to lock the queue in order to prevent double writes and to
    // maintain data integrity:
    pthread_mutex_lock(&(queue->queue_lock));

    task_t* current = queue->head;
    while (current->next) {
        current = current->next;
    }

    if (!current) {
        queue->head = task;
    }
    else {
        current->next = task;
    }

    // Notify the queue that there is a new task and unlock the read/write
    // mutex:
    sem_post(&(queue->sem_new_task));
    pthread_mutex_unlock(&(queue->queue_lock));
}

/**
 * Get the next task from the dispatch queue
 */
task_t* _queue_get_task(dispatch_queue_t* queue) {

    // Lock the queue in order to preserve data integrity:
    pthread_mutex_lock(&(queue->queue_lock));

    task_t* task = NULL;

    if (queue->head) {
        task = queue->head;
        if (task->next) {
            queue->head = task->next;
        }
    }

    pthread_mutex_unlock(&(queue->queue_lock));
    return task;
}

/**
 * Creates a dispatch queue.
 * The queue type is either CONCURRENT or SERIAL.
 */
dispatch_queue_t* dispatch_queue_create(queue_type_t queueType) {
    
    dispatch_queue_t* queue = malloc(sizeof(dispatch_queue_t));
    queue->state = WAITING;
    
    // Initialize the semaphores:
    sem_init(&(queue->sem_new_task), 0, 0);
    sem_init(&(queue->sem_next_thread), 0, 0);
    sem_init(&(queue->sem_wait), 0, 0);

    int thread_status = 0;
    switch (queueType) {
        // A concurrent queue dispatches tasks in the order that they 
        // are added, but they also allow tasks from the same queue
        // to run concurrently. 
        case CONCURRENT:
            queue->queue_type = CONCURRENT;
            thread_status = pthread_create(&(queue->thread_control), NULL,
                _dispatcher_concurrent, queue);
            break;

        // A serial queue dispatches a task and waits for the task to
        // complete before selecting and dispatching the next task.
        case SERIAL:
            queue->queue_type = SERIAL;
            thread_status = pthread_create(&(queue->thread_control), NULL,
                _dispatcher_serial, queue);
            break;

        default:
            error_exit("Invalid queue type.\n");
    }

    if (thread_status) error_exit("Could not create control thread.\n");

    return queue;
}

/**
 * Destroys the dispatch queue.
 */
void dispatch_queue_destroy(dispatch_queue_t* queue) {

    //@@TODO: kill the semaphores, mutexes, etc.


    free(queue);
}

/**
 * Creates a task.
 */
task_t* task_create(void (* work)(void*) , void* params, char* name) {
    
    task_t* task = malloc(sizeof(task_t));

    // Copy the name string into the task, truncating at 64 chars.
    int i;
    for (i = 0; i < 63 && name[i] != '\0'; i++) {
        task->name[i] = name[i];
    }
    task->name[i] = '\0';
    printf("%s", task->name);

    task->work = work;
    task->params = params;

    return task;
}

/**
 * Destroys a task.
 */
void task_destroy(task_t* task) {
    free(task);
}

/**
 * Sends a task to the queue. This function does not return to the calling
 * thread until the task has been completed.
 */
void dispatch_sync(dispatch_queue_t* queue, task_t* task) {

    // Initialize a semaphore on the task so we can wait for it
    sem_t* sem = &(task->sem_task);
    sem_init(sem, 0, 0);

    _queue_add_task(queue, task);

    sem_wait(sem);
}


/**
 * Sends a task to the queue. This function returns immediately; the task
 * will be dispatched sometime in the future.
 */
void dispatch_async(dispatch_queue_t* queue, task_t* task) {
    _queue_add_task(queue, task);
}

/**
 * Waits (blocks) until all tasks on the queue have been completed. If new
 * tasks are added to the queue after this they are ignored. 
 */
int dispatch_queue_wait(dispatch_queue_t* queue) {
    // pthread_join()
}

/**
 * Executes the function 'work' a certain number of times (potentially in
 * parallel, if the queue is concurrent). Each iteration of the work function 
 * is passed an integer from 0 to number-1. This function does not return 
 * until all of the iterations of the work function have completed.
 */ 
void dispatch_for(dispatch_queue_t* queue, long number, void (* work)(long)) {

}