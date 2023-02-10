#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "epoll.h"
#include "HttpHandler.h"
#include "Log.h"
#include "ThreadPool.h"
#include "Utils.h"

using namespace std;

void handleNewConnections(Epoll* epoll, int listen_fd, int* idle_fd)
{
    sockaddr_in client_addr;
    socklen_t client_addr_len = 0;

    for(;;) {
        int client_fd = accept4(listen_fd, (sockaddr*)&client_addr, &client_addr_len,
                                SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(client_fd == -1) {
            if(errno == EINTR || errno == ECONNABORTED)
                continue;
            else if (errno == EAGAIN)
                break;
            else if(errno == EMFILE) {
                int closed_conn_num = closeRemainingConnect(listen_fd, idle_fd);
                WARN("No reliable pipes in new connection, close %d conns", closed_conn_num);
                break;
            }
            else
                ERROR("Accept Error! (%s)", strerror(errno));
        }
        else {

            Timer* timer = new Timer(TFD_NONBLOCK | TFD_CLOEXEC);
            if(!timer->isValid())
            {
                delete timer;
                close(client_fd);

                int closed_conn_num = closeRemainingConnect(listen_fd, idle_fd);
                WARN("No reliable pipes in new connection, close %d conns", closed_conn_num);
                break;
            }
            HttpHandler* client_handler = new HttpHandler(epoll, client_fd, timer);
            bool ret1 = epoll->add(client_fd, client_handler->getClientEpollEvent(), client_handler->getClientTriggerCond());
            bool ret2 = epoll->add(timer->getFd(), client_handler->getTimerEpollEvent(), client_handler->getTimerTriggerCond());
            assert(ret1 && ret2);

            printConnectionStatus(client_fd, "-------->>>>> New Connection");
        }
    }
}


void handleOldConnection(Epoll* epoll, int fd, ThreadPool* thread_pool, epoll_event* event)
{
    EpollEvent* curr_epoll_event = static_cast<EpollEvent*>(event->data.ptr);
    HttpHandler* handler = static_cast<HttpHandler*>(curr_epoll_event->ptr);
    int events_ = event->events;
    if ((events_ & EPOLLHUP) || (events_ & EPOLLRDHUP)) {
        INFO("Socket(%d) was closed by peer.", handler->getClientFd());
        delete handler;
        return;
    }

    else if ((events_ & EPOLLERR) || !(events_ & EPOLLIN)) {
        ERROR("Socket(%d) error.", handler->getClientFd());
        delete handler;
        return;
    }
    // 1. Time out
    if(fd == handler->getTimer()->getFd())
    {
        INFO("-------->>>>> "
             "New Message: socket(%d) - timerfd(%d) timeout."
             " <<<<<--------",
             handler->getClientFd(), handler->getTimer()->getFd());
        delete handler;
    }
    // 2. other error
    else
    {
        epoll->modify(handler->getTimer()->getFd(), nullptr, 0);
        thread_pool->appendTask(
                [](void* arg)
                {
                    HttpHandler* handler = static_cast<HttpHandler*>(arg);

                    printConnectionStatus(handler->getClientFd(), "-------->>>>> New Message");
                    if(!(handler->RunEventLoop()))
                        delete handler;
                },
                handler);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2 || !isNumericStr(argv[1]))
    {
        ERROR("usage: %s <port> [<www_dir>]", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    if(argc > 2)
        HttpHandler::setWWWPath(argv[2]);

    INFO("PID: %d", getpid());
    handleSigpipe();
    ThreadPool thread_pool(8);

    int idle_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    int listen_fd = -1;
    if((listen_fd = socket_bind_and_listen(port)) == -1)
    {
        ERROR("Bind %d port failed ! (%s)", port, strerror(errno));
        exit(EXIT_FAILURE);
    }

    Epoll epoll(EPOLL_CLOEXEC);
    assert(epoll.isEpollValid());
    EpollEvent* listen_epollevent = new EpollEvent{listen_fd, nullptr};
    epoll.add(listen_fd, listen_epollevent, EPOLLET | EPOLLIN);

    for(;;)
    {
        int event_num = epoll.wait(-1);

        if(event_num < 0)
        {
            assert(event_num != -2);

            if(errno == EINTR)
                continue;
            else
                FATAL("epoll_wait fail! (%s)", strerror(errno));
        }
        else if(event_num == 0)
            continue;

        for(int i = 0; i < event_num; i++)
        {
            epoll_event&& event = epoll.getEvent(static_cast<size_t>(i));
            EpollEvent* curr_epoll_event = static_cast<EpollEvent*>(event.data.ptr);
            int fd = curr_epoll_event->fd;
            if(fd == listen_fd)
                handleNewConnections(&epoll, listen_fd, &idle_fd);
            else
                handleOldConnection(&epoll, fd, &thread_pool, &event);
        }
    }
    delete listen_epollevent;

    return 0;
}