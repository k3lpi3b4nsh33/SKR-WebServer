//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_THREADPOOL_H
#define WEBSERVER_THREADPOOL_H

#include <cassert>
#include <queue>

#include "Condition.h"
#include "MutexLock.h"

using namespace std;

class ThreadPool
{
public:
    /**
     * Two types of shutdown
     * 1. wait events done and shutdown
     * 2. shutdown NOW
     */
    enum ShutdownMode { GRACEFUL_QUIT, IMMEDIATE_SHUTDOWN } ;

    /***
     * @brief   Create ThreadPool
     * @param   threadNum       the size of ThreadPool
     * @param   shutdown_mode   Shutdown mode
     * @param   maxQueueSize    the max queue size of ThreadPoll, default is -1
     */
    ThreadPool( size_t threadNum,
                ShutdownMode shutdown_mode = GRACEFUL_QUIT,
                size_t maxQueueSize = -1
    );

    ~ThreadPool();

    /***
     * @brief   Adds the current task to the thread pool
     */
    bool appendTask(void (*function)(void*), void* arguments);

private:
    /**
     * @brief The function to be executed by each child thread in while the event queue is polled.
     */
    static void* TaskForWorkerThreads_(void* arg);

    /***
     * Basic Task
     */
    struct ThreadpoolTask
    {
        void (*function)(void*);
        void* arguments;
    };

    size_t threadNum_;                          // The number of threads
    size_t maxQueueSize_;                       // The max size of queue. if the size exceeded, stop adding tasks
    queue<ThreadpoolTask> task_queue_;          // Task queue
    vector<pthread_t> threads_;                 // The flag of thread

    // Thread lock. Ensure that only one thread in the thread poll works at the same time
    MutexLock threadpool_mutex_;
    // Condition variable of thread pool. Waking up the free thread when task is adding in thread pool
    Condition threadpool_cond_;
    // When the thread pool is destroyed, the shutdown mode of last threads
    ShutdownMode  shutdown_mode_;

};


#endif //WEBSERVER_THREADPOOL_H
