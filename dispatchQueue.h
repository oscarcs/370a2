/* 
 * File:   dispatchQueue.h
 * Author: robert
 *
 * Modified by: osim082
 */

#ifndef DISPATCHQUEUE_H
#define	DISPATCHQUEUE_H

#include <pthread.h>
#include <semaphore.h>
    
#define error_exit(MESSAGE)     perror(MESSAGE), exit(EXIT_FAILURE)

    typedef enum { // whether dispatching a task synchronously or asynchronously
        ASYNC, SYNC
    } task_dispatch_type_t;
    
    typedef enum { // The type of dispatch queue.
        CONCURRENT, SERIAL
    } queue_type_t;

    typedef enum { // State of a queue: running, waiting, or finished.
        RUNNING, WAITING, FINISHED
    } queue_state_t;

    typedef struct task {
        char name[64];              // Name to identify task when debugging
        void (* work)(void*);       // The function to perform
        void* params;               // Parameters to pass to the function
        task_dispatch_type_t type;  // Asynchronous / synchronous?
        sem_t sem_task;             // Semaphore for synchronous dispatch
        struct task* next;          // Pointer to the next task in the list
        struct task* prev;          // Pointer to the previous task in the list
    } task_t;
    
    typedef struct dispatch_queue_t dispatch_queue_t; // The dispatch queue type
    typedef struct dispatch_queue_thread_t dispatch_queue_thread_t; // The dispatch queue thread type

    struct dispatch_queue_thread_t {
        dispatch_queue_t* queue;    // The queue this thread is associated with
        pthread_t thread;           // The thread which runs the task
        sem_t thread_semaphore;     // The semaphore the thread waits on until a task is allocated
        task_t* task;               // The current task for this thread
    };

    struct dispatch_queue_t {
        queue_type_t queue_type;            // The type of queue - serial or concurrent
        queue_state_t state;                // Current state of the queue.
        task_t* head;                       // First task in the queue (head of the linked list).
        dispatch_queue_thread_t* threads;   // Array of threads to run tasks on.
        // pthread_t thread_control;           // Controls the queue.
        pthread_mutex_t queue_lock;         // Lock for reading and writing to the queue.
        sem_t sem_new_task;                 // Semaphore for pending tasks.
        sem_t sem_next_thread;              // Used to wait for a thread to become available.
        sem_t sem_wait;                     // Semaphore used to wait for thread to finish.
    };
    
    task_t* task_create(void (*)(void*), void*, char*);
    
    void task_destroy(task_t*);

    dispatch_queue_t* dispatch_queue_create(queue_type_t);
    
    void dispatch_queue_destroy(dispatch_queue_t*);
    
    void dispatch_async(dispatch_queue_t*, task_t*);
    
    void dispatch_sync(dispatch_queue_t*, task_t*);
    
    void dispatch_for(dispatch_queue_t*, long, void (*)(long));
    
    int dispatch_queue_wait(dispatch_queue_t*);

#endif	/* DISPATCHQUEUE_H */