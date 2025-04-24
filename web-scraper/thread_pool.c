#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

// Worker thread function
static void *worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t *)arg;
    
    while (1) {
        pthread_mutex_lock(&pool->queue_mutex);
        
        // Wait for task or shutdown
        while (pool->queue_count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_not_empty, &pool->queue_mutex);
        }
        
        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->queue_mutex);
            pthread_exit(NULL);
        }
        
        // Get task from queue
        task_t *task = &pool->queue[pool->queue_front];
        pool->queue_front = (pool->queue_front + 1) % pool->queue_size;
        pool->queue_count--;
        pool->active_tasks++;
        
        pthread_cond_signal(&pool->queue_not_full);
        pthread_mutex_unlock(&pool->queue_mutex);
        
        // Execute task
        task->function(task->arg);
        
        // Task completed
        pthread_mutex_lock(&pool->queue_mutex);
        pool->active_tasks--;
        pthread_cond_signal(&pool->queue_not_full);
        pthread_mutex_unlock(&pool->queue_mutex);
    }
    
    return NULL;
}

// Create a new thread pool
thread_pool_t *thread_pool_create(int num_threads, int queue_size) {
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    if (!pool) return NULL;
    
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->queue = malloc(sizeof(task_t) * queue_size);
    if (!pool->threads || !pool->queue) {
        free(pool->threads);
        free(pool->queue);
        free(pool);
        return NULL;
    }
    
    pool->thread_count = num_threads;
    pool->queue_size = queue_size;
    pool->queue_count = 0;
    pool->queue_front = 0;
    pool->queue_rear = 0;
    pool->shutdown = false;
    pool->active_tasks = 0;
    
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_not_empty, NULL);
    pthread_cond_init(&pool->queue_not_full, NULL);
    
    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            // Cleanup on error
            pool->shutdown = true;
            pthread_cond_broadcast(&pool->queue_not_empty);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            thread_pool_destroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Destroy thread pool
void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = true;
    pthread_mutex_unlock(&pool->queue_mutex);
    
    pthread_cond_broadcast(&pool->queue_not_empty);
    
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    free(pool->threads);
    free(pool->queue);
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_not_empty);
    pthread_cond_destroy(&pool->queue_not_full);
    free(pool);
}

// Add task to the pool
bool thread_pool_add_task(thread_pool_t *pool, void *(*function)(void *), void *arg) {
    if (!pool || !function) {
        return false;
    }

    pthread_mutex_lock(&pool->queue_mutex);

    // Wait if queue is full
    while (pool->queue_count == pool->queue_size && !pool->shutdown) {
        pthread_cond_wait(&pool->queue_not_full, &pool->queue_mutex);
    }

    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->queue_mutex);
        return false;
    }

    // Add task to queue
    pool->queue[pool->queue_rear].function = function;
    pool->queue[pool->queue_rear].arg = arg;
    pool->queue_rear = (pool->queue_rear + 1) % pool->queue_size;
    pool->queue_count++;

    pthread_cond_signal(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_mutex);

    return true;
}

// Wait for all tasks to complete
void thread_pool_wait(thread_pool_t *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->queue_mutex);
    while (pool->queue_count > 0 || pool->active_tasks > 0) {
        pthread_cond_wait(&pool->queue_not_full, &pool->queue_mutex);
    }
    pthread_mutex_unlock(&pool->queue_mutex);
}

// Get current number of tasks in queue
int thread_pool_get_queue_size(thread_pool_t *pool) {
    if (!pool) return 0;
    
    pthread_mutex_lock(&pool->queue_mutex);
    int size = pool->queue_count;
    pthread_mutex_unlock(&pool->queue_mutex);
    
    return size;
} 