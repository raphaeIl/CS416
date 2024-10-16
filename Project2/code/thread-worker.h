// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:
// gcc test.c ../thread-worker.c ../tscheduler.c ../queue.c -o test
#ifndef WORKER_T_H
#define WORKER_T_H

#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the USE_WORKERS macro */
#define USE_WORKERS 1

/* include lib header files that you need here: */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <string.h>

typedef uint worker_t;
typedef struct queue_t queue_t;

typedef struct TCB {
	/* add important states in a thread control block */
	int threadId; // thread Id
	int status; // thread status - running, waiting, blocked, ready
	ucontext_t* context; // thread context
	void* stack; // thread stack
	int priority; // thread priority
	// And more ...

	// YOUR CODE HERE
} tcb; 

/* mutex struct definition */
typedef struct worker_mutex_t {
	/* add something here */
	int locked;
	worker_t owner;

	queue_t* blocked_threads;
// YOUR CODE HERE
} worker_mutex_t;

/* Priority definitions */
#define NUMPRIO 4

#define HIGH_PRIO 3
#define MEDIUM_PRIO 2
#define DEFAULT_PRIO 1
#define LOW_PRIO 0

/* Priority definitions */
#define WAITING_STATUS 0
#define READY_STATUS 1
#define RUNNING_STATUS 2
#define BLOCKED_STATUS 3
#define FINISHED_STATUS 4

#define STACK_SIZE SIGSTKSZ

#define MAX_THREADS 100
#define NUM_QUEUES 3

#define TIME_QUANTUM 900 // (in ms)

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue etc...)

// YOUR CODE HERE
struct queue_t{
    tcb* threads[MAX_THREADS];
    int front;
    int back;
};

typedef struct {
	queue_t* queues[NUM_QUEUES];
} linkedlist_t;

typedef struct {
    linkedlist_t* run_queue;

    ucontext_t* main_context; // main context idk where to store this
    ucontext_t* scheduler_context; // scheduler context
	void* scheduler_stack; // scheduler context stack

	tcb* main_thread;
	tcb* current_thread; // currently running thread;
} scheduler_t;

/*
	a wrapper like this is necessary since i don't think we can just pass our original
	thread function like `simulate_long_task` directly into makecontext because at the end
	of the function we need to swap the context back so i'm adding a wrapper to do that: thread_function_wrapper
	
	there is probably a better solutinon to this lol
 */ 
typedef struct {
	void *(*original_function)(void*); // original worker function (for example: simulate_long_task)
	int* original_args;  // original function's args
} worker_wrapper_t;

/* Function Declarations: */

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, void
    *(*function)(void*), void * arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, const pthread_mutexattr_t
    *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

void ll_init(linkedlist_t* ll);

void q_init(queue_t* q);

void q_enqueue(queue_t* q, tcb* item);

tcb* q_dequeue(queue_t* q);

tcb* q_peek(queue_t* q);

int q_is_empty(queue_t* q);

void q_printqueue(queue_t* q);

void q_destroy(queue_t* q);

void sch_init(scheduler_t* scheduler);

void sch_switch();

void sch_schedule(scheduler_t* scheduler, tcb* thread);
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

/* Util Functions */
void create_timer(time_t duration, int repeat, __sighandler_t on_expire_handler);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#define pthread_setschedprio worker_setschedprio
#endif

#endif
