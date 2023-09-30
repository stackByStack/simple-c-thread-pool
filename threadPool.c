#include "threadPool.h"

void init_pool(ThreadPool* pool) {
    pool->front = 0;
    pool->rear = 0;
    pool->count = 0;
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    pthread_cond_init(&pool->not_full, NULL);
    pthread_cond_init(&pool->all_tasks_completed, NULL);
    pool->shutdown = 0; // Initialize shutdown flag
    pool->pending_tasks = 0; // Initialize pending task counter
}

void destroy_pool(ThreadPool* pool) {
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);
    pthread_cond_destroy(&pool->all_tasks_completed);
}

void enqueue_task(ThreadPool* pool, Task* task) {
    pthread_mutex_lock(&pool->mutex);
    while (pool->count == MAX_TASKS) {
        pthread_cond_wait(&pool->not_full, &pool->mutex);
    }
    pool->tasks[pool->rear] = task;
    pool->rear = (pool->rear + 1) % MAX_TASKS;
    pool->count++;
    pool->pending_tasks++; // Increment pending task counter
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
}

Task* dequeue_task(ThreadPool* pool) {
    Task* task = NULL;
    pthread_mutex_lock(&pool->mutex);
    while (pool->count == 0 && !pool->shutdown) {
        pthread_cond_wait(&pool->not_empty, &pool->mutex);
    }
    if (pool->shutdown) { // Check if shutdown requested
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }
    task = pool->tasks[pool->front];
    pool->front = (pool->front + 1) % MAX_TASKS;
    pool->count--;
    pthread_cond_signal(&pool->not_full);
    pthread_mutex_unlock(&pool->mutex);
    return task;
}

void* worker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    while (1) {
        Task* task = dequeue_task(pool);
        if (task == NULL) { // Break the loop if shutdown requested
            break;
        }
        task->task(task->arg);
        free(task);
        pthread_mutex_lock(&pool->mutex);
        pool->pending_tasks--; // Decrement pending task counter
        if (pool->pending_tasks == 0) { // If all tasks completed, signal the main thread
            pthread_cond_signal(&pool->all_tasks_completed);
        }
        pthread_mutex_unlock(&pool->mutex);
    }
    return NULL;
}

void submit_task(ThreadPool* pool, void (*task)(void* arg), void* arg) {
    Task* newTask = (Task*)malloc(sizeof(Task));
    newTask->task = task;
    newTask->arg = arg;
    enqueue_task(pool, newTask);
}

// set the thread pool to be shutdown after all tasks are completed
void shutdown_pool(ThreadPool* pool) {
    pthread_mutex_lock(&pool->mutex);
    while (pool->pending_tasks > 0) { // Wait until all tasks are completed
        pthread_cond_wait(&pool->all_tasks_completed, &pool->mutex);
    }
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
}
