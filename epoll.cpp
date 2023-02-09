//
// Created by kelpie on 2/3/23.
//
#include <cassert>
#include <iostream>
#include <unistd.h>

#include "epoll.h"
#include "Log.h"

Epoll::Epoll(int flag) : epoll_fd_(-1)
{
    create(flag);
}

Epoll::~Epoll()
{
    destroy();
}

bool Epoll::isEpollValid()
{
    return epoll_fd_ >= 0;
}

// Create an epoll object
bool Epoll::create(int flag)
{
    if(!isEpollValid()
       && ((epoll_fd_ = epoll_create1(flag)) == -1))
    {
        ERROR("Create Epoll fail! (%s)", strerror(errno));
        return false;
    }
    return true;
}

bool Epoll::add(int fd, void* data, int event)
{
    if(isEpollValid())
    {
        epoll_event ep_event;
        ep_event.events = event;
        ep_event.data.ptr = data;
        return (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ep_event) != -1);
    }
    return false;
}

bool Epoll::modify(int fd, void* data, int event)
{
    if(isEpollValid())
    {
        epoll_event ep_event;
        ep_event.events = event;
        ep_event.data.ptr = data;
        return (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ep_event) != -1);
    }
    return false;
}

bool Epoll::del(int fd)
{
    if(isEpollValid())
        return (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) != -1);
    return false;
}

// Waits for any of the events registered for with epoll_ctl
int Epoll::wait(int timeout)
{
    if(isEpollValid())
        return epoll_wait(epoll_fd_, events_, MAX_EVENTS, timeout);
    return -2;
}

void Epoll::destroy()
{
    if(isEpollValid())
        close(epoll_fd_);
    epoll_fd_ = -1;
}

// return an const pointer
epoll_event Epoll::getEvent(size_t index)
{
    assert(index < MAX_EVENTS);
    return events_[index];
}
