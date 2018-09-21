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
// #define NUM_CORES 3

void _thread_worker(dispatch_queue_thread_t*);
void _queue_add_task(dispatch_queue_t*, task_t*);
task_t* _queue_pop_task(dispatch_queue_t*);
int _queue_get_length(dispatch_queue_t*);

/**
 * The main dispatch function run by the control thread of this queue.
 * We 
 */
void _thread_worker(dispatch_queue_thread_t* thread) {
    
    for (;;) {

        // Wait for a task to arrive
        sem_wait(&(thread->queue->sem_new_task));
        
        int sem_tasks, actual_tasks;
        sem_getvalue(&(thread->queue->sem_new_task), &sem_tasks);
        actual_tasks = _queue_get_length(thread->queue);
        printf("\tsemaphore %i (%i), actual (%i)\n", sem_tasks, sem_tasks+1, actual_tasks);

        int end = 1;

        // If there are actually tasks, we run then
        if (_queue_get_length(thread->queue) > 0) {            
            printf("\tNew task recieved!\n");

            // Get the task
            task_t* task = _queue_pop_task(thread->queue);

            if (task != NULL) {
                printf("\tTask is called %s\n", task->name);
                end = 0;

                // Run the task:
                task->work(task->params);

                // If the task has a semaphore, post to it
                if (task->type == SYNC) {
                    sem_post(&(task->sem_task));
                }
                
                // Destroy the task
            }
            
        }

        if (end) {
            printf("\tPosting and exiting...\n");
            sem_post(&(thread->queue->sem_end));
            break;
        }
    }
    
    //@@TODO: clean up stuff.
}

/**
 * Helper function to append a task to a queue.
 */
void _queue_add_task(dispatch_queue_t* queue, task_t* task) {

    // We need to lock the queue in order to prevent double writes and to
    // maintain data integrity:
    pthread_mutex_lock(&(queue->queue_lock));

    if (!queue->head) {
        queue->head = task;
    }
    else {
        task_t* current = queue->head;
        while (current->next) {
            current = current->next;
        }
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
task_t* _queue_pop_task(dispatch_queue_t* queue) {

    // Lock the queue in order to preserve data integrity:
    pthread_mutex_lock(&(queue->queue_lock));

    task_t* task = NULL;

    if (queue->head) {
        task = queue->head;
                
        if (task->next) {
            queue->head = task->next;
        }
        else {
            queue->head = NULL;
        }
    }

    pthread_mutex_unlock(&(queue->queue_lock));
    return task;
}

/**
 * Get the length of the queue.
 */
int _queue_get_length(dispatch_queue_t* queue) {
    
    // Lock the queue to preserve data integrity.
    pthread_mutex_lock(&(queue->queue_lock));

    int count = 0;
    if (queue->head) {
        count++;
        task_t* current = queue->head;
        while (current->next) {
            current = current->next;
            count++;
        }
    }

    pthread_mutex_unlock(&(queue->queue_lock));
    return count;
}

/**
 * Creates a dispatch queue.
 * The queue type is either CONCURRENT or SERIAL.
 */
dispatch_queue_t* dispatch_queue_create(queue_type_t queueType) {
    
    dispatch_queue_t* queue = malloc(sizeof(dispatch_queue_t));
    // queue->state = WAITING;

    // Initialize the semaphores:
    if (sem_init(&(queue->sem_new_task), 0, 0) ||
        sem_init(&(queue->sem_end), 0, 0)
    ) {
        error_exit("Semaphore could not be initialized.\n");
    }

    queue->allow_additional_writes = 1; 

    int threads_to_create = 0;
    int thread_status = 0;
    switch (queueType) {
        // A concurrent queue dispatches tasks in the order that they are 
        // added, but they also allow tasks from the same queueto run 
        // concurrently. 
        case CONCURRENT:
            queue->queue_type = CONCURRENT;
            threads_to_create = NUM_CORES;
             break;

        // A serial queue dispatches a task and waits for the task to
        // complete before selecting and dispatching the next task. Hence, it
        // will have only one worker thread.
        case SERIAL:
            queue->queue_type = SERIAL;
            threads_to_create = 1;
            break;

        default:
            error_exit("Invalid queue type.\n");
    }
    printf("\tCreating %i threads\n", threads_to_create);

    dispatch_queue_thread_t* threads 
        = malloc(sizeof(dispatch_queue_thread_t) * threads_to_create);
    queue->threads = threads;
    
    // Initialize the threads in the threadpool.
    for (int i = 0; i < threads_to_create && !thread_status; i++) {

        // Set the reference to the parent
        threads[i].queue = queue;

        // Finally, start the thread running:
        thread_status = pthread_create(&(queue->threads[i].thread), NULL,
            (void * (*)(void *)) _thread_worker, &queue->threads[i]);    
    }

    if (thread_status) error_exit("Could not create thread.\n");

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
    printf("\tCreating new task '%s'\n", task->name);

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

    if (!queue->allow_additional_writes) {
        return;
    }

    // Initialize a semaphore on the task so we can wait for it
    sem_t* sem = &(task->sem_task);
    sem_init(sem, 0, 0);

    task->type = SYNC;
    _queue_add_task(queue, task);

    sem_wait(sem);
}


/**
 * Sends a task to the queue. This function returns immediately; the task
 * will be dispatched sometime in the future.
 */
void dispatch_async(dispatch_queue_t* queue, task_t* task) {
    
    if (!queue->allow_additional_writes) {
        return;
    }

    task->type = ASYNC;
    _queue_add_task(queue, task);
}

/**
 * Waits (blocks) until all tasks on the queue have been completed. If new
 * tasks are added to the queue after this they are ignored. 
 */
void dispatch_queue_wait(dispatch_queue_t* queue) {
    
    // Stop the queue from being written to:
    queue->allow_additional_writes = 0;    

    int num_threads;
    switch (queue->queue_type) {
        case CONCURRENT:
            num_threads = NUM_CORES;
            break;
        case SERIAL:
            num_threads = 1;
            break;
    }

    // Signal to all of the workers that we are finished.
    // Each thread will get tasks
    for (int i = 0; i < num_threads; i++) {
        sem_post(&(queue->sem_new_task));
    }

    printf("\tWaiting for tasks to complete...\n");

    // Wait for the tasks to finish
    for (int i = 0; i < num_threads; i++) {
        sem_wait(&(queue->sem_end));
    }
}

/**
 * Executes the function 'work' a certain number of times (potentially in
 * parallel, if the queue is concurrent). Each iteration of the work function 
 * is passed an integer from 0 to number-1. This function does not return 
 * until all of the iterations of the work function have completed.
 */ 
void dispatch_for(dispatch_queue_t* queue, long number, void (* work)(long)) {

    // Create tasks and asynchronously add them to the queue:
    for (int i = 0; i < number; i++) {
        
        char name[63];
        sprintf(name, "%d", i);

        task_t* task = task_create((void (* )(void*)) work, i, name);

        dispatch_async(queue, task);
    }    

    // Wait for the tasks to complete.
    dispatch_queue_wait(queue);
}