//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_CONDITION_H
#define WEBSERVER_CONDITION_H

#include <cerrno>
#include <pthread.h>

#include "MutexLock.h"

class Condition
{
private:
    MutexLock& lock_;
    pthread_cond_t cond_;

public:
    Condition(MutexLock& mutex) : lock_(mutex) { pthread_cond_init(&cond_, nullptr); }
    ~Condition()        { pthread_cond_destroy(&cond_); }

    void notify()       { pthread_cond_signal(&cond_); }
    void notifyAll()    { pthread_cond_broadcast(&cond_); }
    void wait()         { pthread_cond_wait(&cond_, lock_.getMutex()); }

    bool waitForSecond(size_t sec)
    {
        timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        abstime.tv_sec += (time_t) sec;
        return ETIMEDOUT != pthread_cond_timedwait(&cond_, lock_.getMutex(), &abstime);
    }
};

#endif //WEBSERVER_CONDITION_H
