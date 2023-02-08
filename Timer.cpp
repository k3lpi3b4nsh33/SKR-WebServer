//
// Created by kelpie on 2/3/23.
//

#include <unistd.h>

#include "Log.h"
#include "Utils.h"
#include "Timer.h"

Timer::Timer(int flag, time_t sec, long nsec) : timer_fd_(-1)
{
    if(create(flag))
        setTime(sec, nsec);
}

Timer::~Timer()
{
    cancel();
    destroy();
}

int Timer::getFd()
{
    return timer_fd_;
}

bool Timer::isValid()
{
    return timer_fd_ >= 0;
}

bool Timer::create(int flag)
{
    if(!isValid() && ((timer_fd_ = timerfd_create(CLOCK_BOOTTIME, flag)) == -1))
        return false;

    return true;
}

bool Timer::setTime(time_t sec, long nsec)
{
    struct itimerspec timerspec;
    memset(&timerspec, 0, sizeof(timerspec));

    timerspec.it_value.tv_nsec = nsec;
    timerspec.it_value.tv_sec = sec;
    if(!isValid() || (timerfd_settime(timer_fd_, 0, &timerspec, nullptr) == -1))
        return false;
    return true;
}

bool Timer::cancel()
{
    return setTime(0,0);
}
void Timer::destroy()
{
    if(isValid())
        close(timer_fd_);
    timer_fd_ = -1;
}

timespec Timer::getNextTimeout()
{
    itimerspec nextTime;
    if(!isValid() || (timerfd_gettime(timer_fd_, &nextTime) == -1))
    {
        ERROR("Timer getNextTimeout fail! (%s)", strerror(errno));
        timespec ret;
        ret.tv_sec = ret.tv_nsec = -1;
        return ret;
    }
    return nextTime.it_interval;
}
