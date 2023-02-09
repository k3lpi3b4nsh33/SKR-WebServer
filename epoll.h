//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_EPOLL_H
#define WEBSERVER_EPOLL_H

#include <sys/epoll.h>
#include "Utils.h"

using namespace std;
struct EpollEvent
{
    int fd;     // file descriptor
    void* ptr;  // data
};

class Epoll
{
public:

    Epoll(int flag = 0);
    ~Epoll();

// Epoll operation
    bool isEpollValid();
    bool create(int flag = 0);
    bool add(int fd, void* data, int event);
    bool modify(int fd, void* data, int event);
    bool del(int fd);
    int wait(int timeout);

    void destroy();
    epoll_event getEvent(size_t index);

private:
    static const size_t MAX_EVENTS = 4096;

    int epoll_fd_;
    epoll_event events_[MAX_EVENTS];
};



#endif //WEBSERVER_EPOLL_H
