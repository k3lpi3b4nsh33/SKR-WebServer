//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_MUTEXLOCK_H
#define WEBSERVER_MUTEXLOCK_H

#include <pthread.h>

/**
 * MutexLock encapsulates pthread_mutex into a class
 */
class MutexLock
{
private:
    pthread_mutex_t mutex_;
public:
    MutexLock()     { pthread_mutex_init(&mutex_, nullptr); }
    ~MutexLock()    { pthread_mutex_destroy(&mutex_); }
    void lock()     { pthread_mutex_lock(&mutex_); }
    void unlock()   { pthread_mutex_unlock(&mutex_); }
    pthread_mutex_t* getMutex() { return &mutex_; };
};

/**
 * MutexLockGuard encapsulates MutexLock into a class
 * Make it easier for us to understand the code
 */
class MutexLockGuard
{
private:
    MutexLock& lock_;
public:
    MutexLockGuard(MutexLock& mutex) : lock_(mutex) { lock_.lock(); }
    ~MutexLockGuard() { lock_.unlock(); }
};


#endif //WEBSERVER_MUTEXLOCK_H
