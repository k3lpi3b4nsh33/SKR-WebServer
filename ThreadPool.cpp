//
// Created by kelpie on 2/3/23.
//

#include "Log.h"
#include "ThreadPool.h"
#include "Utils.h"

/**
 * @brief Initialize thread pool
 */
ThreadPool::ThreadPool(size_t threadNum, ShutdownMode shutdown_mode, size_t maxQueueSize)
        : threadNum_(threadNum),
          maxQueueSize_(maxQueueSize),
          threadpool_cond_(threadpool_mutex_),
          shutdown_mode_(shutdown_mode)
{
    // Create the thread
    while(threads_.size() < threadNum_)
    {
        pthread_t thread;
        if(!pthread_create(&thread, nullptr, TaskForWorkerThreads_, this))
        {
            threads_.push_back(thread);
        }
    }
}

/**
 * @brief Destroy the Thread poll
 */
ThreadPool::~ThreadPool()
{
    // 向任务队列中添加退出线程事件,注意上锁
    // 注意在 cond 使用之前一定要上 mutex
    {
        // when processing the task queue, we have to lock the thread
        MutexLockGuard guard(threadpool_mutex_);

        // if destroy the thread pool now, make sure the task queue is empty
        if(shutdown_mode_ == IMMEDIATE_SHUTDOWN)
            while(!task_queue_.empty())
                task_queue_.pop();

        // Adding the exit thread in the queue
        for(size_t i = 0; i < threadNum_; i++)
        {
            auto pthreadExit = [](void*) { pthread_exit(0); };
            ThreadpoolTask task = { pthreadExit, nullptr };
            task_queue_.push(task);
        }
        // Waking up all threads and quiting the queue.
        threadpool_cond_.notifyAll();
    }
    for(size_t i = 0; i < threadNum_; i++)
    {
        // recycling the resource of thread
        pthread_join(threads_[i], nullptr);
    }
}

bool ThreadPool::appendTask(void (*function)(void*), void* arguments)
{
    // When operating the task queue, we have to lock the thread
    MutexLockGuard guard(threadpool_mutex_);
    // more than queue size, just withdraw it
    if(task_queue_.size() > maxQueueSize_)
        return false;
    else
    {
        ThreadpoolTask task = { function, arguments };
        task_queue_.push(task);
        threadpool_cond_.notify();
        return true;
    }
}

void* ThreadPool::TaskForWorkerThreads_(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    ThreadpoolTask task;
    for(;;)
    {
        // Get the event
        {
            // when getting the event, we have to lock the thread.
            MutexLockGuard guard(pool->threadpool_mutex_);

            /**
             * If the lock is finally obtained,
             * but no event can be executed,
             * fall asleep, release the lock, and wait for wake-up
             *
             * NOTE: Attention, pthread_cond_Signal will wake up at least one thread
             *       In other word, there may be situations where the wakened thread still has no event processing
             *       At this time, just wait in cycle
             */
            while(pool->task_queue_.size() == 0)
                pool->threadpool_cond_.wait();

            assert(pool->task_queue_.size() != 0);
            task = pool->task_queue_.front();
            pool->task_queue_.pop();
        }

        (task.function)(task.arguments);
    }
    UNREACHABLE();
    return nullptr;
}