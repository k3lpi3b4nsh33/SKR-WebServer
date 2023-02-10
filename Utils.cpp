#include <arpa/inet.h>
#include <cctype>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "Log.h"
#include "MutexLock.h"
#include "Utils.h"

/**
 *
 * @brief The basic server functions in this file.
 *
 */



// socket create
int socket_bind_and_listen(int port)
{
    int listen_fd = 0;
    // AF_INET      : IPv4 Internet protocols  
    // SOCK_STREAM  : TCP socket
    if((listen_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) == -1)
        return -1;

    sockaddr_in server_addr;
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);


    int opt = 1;
    if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        return -1;
    if(bind(listen_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
        return -1;
    if(listen(listen_fd, 1024) == -1)
        return -1;

    return listen_fd;
}

// set the flag in fd
bool setFdNoBlock(int fd)
{
    int flag = fcntl(fd, F_GETFD);
    if(flag == -1)
        return -1;
    flag |= O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flag) == -1)
        return false;
    return true;
}

bool setSocketNoDelay(int fd)
{
    int enable = 1;
    if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable, sizeof(enable)) == -1)
        return false;
    return true;
}

/**
 * @brief read the file
 * @param fd file descriptor
 * @param buf data
 * @param len the len of data
 * @return
 */
ssize_t readn(int fd, void* buf, size_t len)
{
    char *pos = (char*)buf;
    size_t leftNum = len;
    ssize_t readNum = 0;
    while(leftNum > 0)
    {
        ssize_t tmpRead = read(fd, pos, leftNum);
        if(tmpRead < 0)
        {
            if(errno == EINTR)
                tmpRead = 0;
            else if (errno == EAGAIN)
                return readNum;
            else
                return -1;
        }
        // tmpread == 0 ,the connection is close
        if(tmpRead == 0)
            break;
        readNum += tmpRead;
        pos += tmpRead;

        leftNum -= tmpRead;
    }
    return readNum;
}

/**
 * @brief write something in file
 * @param fd
 * @param buf
 * @param len
 * @param isWrite
 * @return
 */
ssize_t writen(int fd, const void* buf, size_t len, bool isWrite)
{
    char *pos = (char*)buf;
    size_t leftNum = len;
    ssize_t writtenNum = 0;
    while(leftNum > 0)
    {
        ssize_t tmpWrite = 0;

        if(isWrite)
            tmpWrite = write(fd, pos, leftNum);
        else
            tmpWrite = send(fd, pos, leftNum, 0);

        if(tmpWrite < 0)
        {
            if(errno == EINTR || errno == EAGAIN)
                tmpWrite = 0;
            else
                return -1;
        }
        if(tmpWrite == 0)
            break;
        writtenNum += tmpWrite;
        pos += tmpWrite;
        leftNum -= tmpWrite;
    }
    return writtenNum;
}

void handleSigpipe()
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, NULL) == -1)
        ERROR("Ignore SIGPIPE failed! (%s)", strerror(errno));
}

void printConnectionStatus(int client_fd_, string prefix)
{
    sockaddr_in serverAddr, peerAddr;
    socklen_t serverAddrLen = sizeof(serverAddr);
    socklen_t peerAddrLen = sizeof(peerAddr);

    if((getsockname(client_fd_, (struct sockaddr *)&serverAddr, &serverAddrLen) != -1)
       && (getpeername(client_fd_, (struct sockaddr *)&peerAddr, &peerAddrLen) != -1))
        INFO("%s: (socket %d) [Server] %s:%d <---> [Client] %s:%d",
             prefix.c_str(), client_fd_,
             inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port),
             inet_ntoa(peerAddr.sin_addr), ntohs(peerAddr.sin_port));
    else
        ERROR("printConnectionStatus failed ! (%s)", strerror(errno));
}

string escapeStr(const string& str, size_t MAXBUF)
{
    string msg = str;
    // Traverse the string
    for(size_t i = 0; i < msg.length(); i++)
    {
        char ch = msg[i];
        // the char can't print
        if(!isprint(ch))
        {
            // Only process '\r', '\n'
            string substr;
            if(ch == '\r')
                substr = "\\r";
            else if(ch == '\n')
                substr = "\\n";
            else
            {
                char hex[10];
                snprintf(hex, 10, "\\x%02x", static_cast<unsigned char>(ch));
                substr = hex;
            }
            msg.replace(i, 1, substr);
        }
    }
    // output the read data
    if(msg.length() > MAXBUF)
        return msg.substr(0, MAXBUF) + " ... ... ";
    else
        return msg;
}

bool isNumericStr(string str)
{
    for(size_t i = 0; i < str.length(); i++)
        if(!isdigit(str[i]))
            return false;
    return true;
}

size_t closeRemainingConnect(int listen_fd, int* idle_fd) {
    close(*idle_fd);

    size_t count = 0;
    for(;;) {
        int client_fd = accept4(listen_fd, nullptr, nullptr,
                                SOCK_NONBLOCK | SOCK_CLOEXEC);
        if(client_fd == -1 && errno == EAGAIN)
            break;
        close(client_fd);
        ++count;
    }
    *idle_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    return count;
}

bool is_path_parent(const string& parent_path, const string& child_path) {
    bool result = false;
    char* parent_p = nullptr, *child_p = nullptr;
    char separator;

    INFO("resolved parent path: %s", parent_path.c_str());
    INFO("resolved child path: %s", child_path.c_str());

    parent_p = canonicalize_file_name(parent_path.c_str());
    if(!parent_p) {
        ERROR("is_path_parent failed, cannot get parent path [%s] (%s)",
              parent_path.c_str(),
              strerror(errno));
        goto clean_parent;
    }

    child_p = canonicalize_file_name(child_path.c_str());
    if(!child_p) {
        ERROR("is_path_parent failed, cannot get child path [%s] (%s)",
              child_path.c_str(),
              strerror(errno));
        goto clean_child;
    }

    // INFO("resolved parent path: %s", parent_p);
    INFO("resolved path: %s", child_p);

    /**
     * Determine whether there is a directory traversal vulnerability
     *
     * Normal visit:
     *              parent: /usr/wwwroot/www/html
     *              child: /usr/wwwroot/www/html/index.html
     * Trick visit:
     *              parent: /usr/wwwroot/www/html
     *              child:  /usr/wwwroot/www/html/../../../../../../flag
     */

    if(child_p == strstr(child_p, parent_p)) {
        // The parent is in the child, so the child [parent.len] will not cross the boundary
        separator = child_p[strlen(parent_p)];
        if (separator == '\0' || separator == '/')
            return true;
    }

    free(child_p);
    clean_child:
    free(parent_p);
    clean_parent:
    return result;
}