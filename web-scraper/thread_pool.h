#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

// Task structure
typedef struct {
    void *(*function)(void *);  // Function that returns void*
    void *arg;
} task_t;

// Thread pool structure
typedef struct thread_pool {
    pthread_t *threads;
    task_t *queue;
    int thread_count;
    int queue_size;
    int queue_count;
    int queue_front;
    int queue_rear;
    bool shutdown;
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;
    int active_tasks;
} thread_pool_t;

// Create a new thread pool with the specified number of threads
thread_pool_t *thread_pool_create(int num_threads, int queue_size);

// Destroy the thread pool
void thread_pool_destroy(thread_pool_t *pool);

// Add a task to the thread pool
bool thread_pool_add_task(thread_pool_t *pool, void *(*function)(void *), void *arg);

// Wait for all tasks to complete
void thread_pool_wait(thread_pool_t *pool);

// Get current number of tasks in queue
int thread_pool_get_queue_size(thread_pool_t *pool);

#endif // THREAD_POOL_H 