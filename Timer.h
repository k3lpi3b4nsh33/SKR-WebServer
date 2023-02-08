//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_TIMER_H
#define WEBSERVER_TIMER_H

#include <sys/timerfd.h>
class Timer
{
private:
    int timer_fd_;
public:
    Timer(int flag = 0, time_t sec = 0, long nsec = 0);
    ~Timer();

    int getFd();
    void setFd(int fd);

    bool isValid();
    bool create(int flag = 0);
    bool setTime(time_t sec, long nsec);

    bool cancel();
    void destroy();
    timespec getNextTimeout();
};

#endif //WEBSERVER_TIMER_H
