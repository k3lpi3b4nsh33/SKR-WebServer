/**
 * Author: Kelpie
 * Date: 2023/2/8
 *
 * Maintain basic connection, log some correct or wrong detail
 */
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cctype>
#include <fcntl.h>
#include <sstream>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "HttpHandler.h"
#include "Log.h"
#include "Utils.h"

/**
 * Declare the static member variable
 * If the www path has not been set before, set the path to the current working path
 */

string HttpHandler::www_path = ".";

/**
 * Initialize client fd and epoll event
 * Initialize timer fd and epoll event
 *
 *  isKeepAlive = true
 * Under HTTP1.1, the default is continuous connection
 * Unless the client http headers have Connection: close
 */
HttpHandler::HttpHandler(Epoll* epoll, int client_fd, Timer* timer)
        : client_fd_(client_fd), client_event_{client_fd_, this},
          timer_(timer), epoll_(epoll), curr_parse_pos_(0)
{
    isKeepAlive_ = true;
    reset();
    if(timer)
        timer_event_ = {timer->getFd(), this};
}

HttpHandler::~HttpHandler()
{
    bool ret1 = epoll_->del(client_fd_);
    bool ret2 = true;
    if(timer_)
    {
        ret2 = epoll_->del(timer_->getFd());
        delete timer_;
    }
    assert(ret1 && ret2);
    INFO("------------------------ "
         "Connection Closed (socket: %d)"
         "------------------------",
         client_fd_);
    close(client_fd_);
}

/**
 * Reset the all response data and request data
 *
 */
void HttpHandler::reset()
{
    assert(request_.length() >= curr_parse_pos_);
    request_.clear();
    curr_parse_pos_ = 0;
    state_ = STATE_PARSE_URI;
    againTimes_ = maxAgainTimes;
    headers_.clear();
    http_body_.clear();
    if(timer_)
        timer_->setTime(timeoutPerRequest, 0);
}

HttpHandler::ERROR_TYPE HttpHandler::readRequest()
{
    INFO("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
         "- Request Packet -"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ");

    char buffer[MAXBUF];

    while(true)
    {
        ssize_t len = recv(client_fd_, buffer, MAXBUF, MSG_DONTWAIT);
        if(len < 0) {
            if(errno == EAGAIN)
                return ERR_SUCCESS;
            else if(errno == EINTR)
                continue;
            return ERR_READ_REQUEST_FAIL;
        }
        else if(len == 0)
        {
            return ERR_CONNECTION_CLOSED;
        }

        // Assemble the read the data
        string request(buffer, buffer + len);
        INFO("{%s}", escapeStr(request, MAXBUF).c_str());

        request_ += request;
    }
    return ERR_SUCCESS;
}

/**
 * @brief parse the URI
 * @return
 */
HttpHandler::ERROR_TYPE HttpHandler::parseURI()
{
    size_t pos1, pos2;

    pos1 = request_.find("\r\n");
    if(pos1 == string::npos)    return ERR_AGAIN;
    string&& first_line = request_.substr(0, pos1);
    pos1 = first_line.find(' ');
    if(pos1 == string::npos)    return ERR_BAD_REQUEST;
    string methodStr = first_line.substr(0, pos1);

    string output_method = "Method: ";
    if(methodStr == "GET")
        method_ = METHOD_GET;
    else if(methodStr == "POST")
        method_ = METHOD_POST;
    else if(methodStr == "HEAD")
        method_ = METHOD_HEAD;
    else
        return ERR_NOT_IMPLEMENTED;
    INFO("Method: %s", methodStr.c_str());

    // b. check the path
    pos1++;
    pos2 = first_line.find(' ', pos1);
    if(pos2 == string::npos)    return ERR_BAD_REQUEST;

    // get the path
    path_ = www_path + "/" + first_line.substr(pos1, pos2 - pos1);
    // determine the traversal vulnerability
    if(!is_path_parent(www_path, path_))
        return ERR_NOT_FOUND;

    INFO("Path: %s", path_.c_str());

    // c. check the version of http
    pos2++;
    string http_version_str = first_line.substr(pos2, first_line.length() - pos2);
    INFO("HTTP Version: %s", http_version_str.c_str());
    if(http_version_str == "HTTP/1.0")
        http_version_ = HTTP_1_0;
    else if (http_version_str == "HTTP/1.1")
        http_version_ = HTTP_1_1;
    else
        return ERR_HTTP_VERSION_NOT_SUPPORTED;

    // update curr_parse_pos_
    curr_parse_pos_ += first_line.length() + 2;
    return ERR_SUCCESS;
}

HttpHandler::ERROR_TYPE HttpHandler::parseHttpHeader()
{
    INFO("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
         "- Request Info -"
         ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");

    size_t pos1, pos2;
    for(pos1 = curr_parse_pos_;
        (pos2 = request_.find("\r\n", pos1)) != string::npos;
        pos1 = pos2 + 2)
    {
        string&& header = request_.substr(pos1, pos2 - pos1);

        if(header.size() == 0)
        {
            curr_parse_pos_ = pos1 + 2;
            return ERR_SUCCESS;
        }
        pos1 = header.find(' ');

        if(pos1 == string::npos)    return ERR_BAD_REQUEST;

        string&& key = header.substr(0, pos1);

        if(key.size() < 2 || key.back() != ':') return ERR_BAD_REQUEST;
        key.pop_back();

        // key tolower
        transform(key.begin(), key.end(), key.begin(), ::tolower);
        // get the value
        string&& value = header.substr(pos1 + 1);

        INFO("HTTP Header: [%s : %s]", key.c_str(), value.c_str());

        headers_[key] = value;
    }
    return ERR_AGAIN;
}

HttpHandler::ERROR_TYPE HttpHandler::parseBody()
{
    assert(method_ == METHOD_POST);

    auto content_len_iter = headers_.find("content-length");
    if(content_len_iter == headers_.end())
        return ERR_LENGTH_REQUIRED;

    string len_str = content_len_iter->second;
    if(!isNumericStr(len_str))
        return ERR_BAD_REQUEST;

    int len = atoi(len_str.c_str());

    if(request_.length() < curr_parse_pos_ + len)
        return ERR_AGAIN;
    http_body_ = request_.substr(curr_parse_pos_, len);

    // output the last body
    INFO("HTTP Body: {%s}", escapeStr(http_body_, MAXBUF).c_str());

    return ERR_SUCCESS;
}

HttpHandler::ERROR_TYPE HttpHandler::handleRequest()
{
    // continuous connection only in HTTP 1.0
    if(http_version_ == HTTP_1_0)
        isKeepAlive_ = false;

    auto conHeaderIter = headers_.find("connection");
    if(conHeaderIter != headers_.end())
    {
        string value = conHeaderIter->second;
        transform(value.begin(), value.end(), value.begin(), ::tolower);
        if(value == "keep-alive")
            isKeepAlive_ = true;
    }

    // get the file detail
    struct stat st;
    if(stat(path_.c_str(), &st) == -1)
    {
        WARN("Can not get file [%s] state ! (%s)", path_.c_str(), strerror(errno));
        if(errno == ENOENT)
            return ERR_NOT_FOUND;
        else
            return ERR_INTERNAL_SERVER_ERR;
    }
    // If try to visit the directory, default visit the index.html
    if (S_ISDIR(st.st_mode)) {
        path_ += "/index.html";
        if(stat(path_.c_str(), &st) == -1)
        {
            WARN("Can not get file [%s] state ! (%s)", path_.c_str(), strerror(errno));
            if(errno == ENOENT)
                return ERR_NOT_FOUND;
            else
                return ERR_INTERNAL_SERVER_ERR;
        }
    }

    // process request
    // GET OR HEAD
    if(method_ == METHOD_GET || method_ == METHOD_HEAD)
    {
        // Open the file
        int file_fd;
        if((file_fd = open(path_.c_str(), O_RDONLY, 0)) == -1)
        {
            WARN("File [%s] open failed ! (%s)", path_.c_str(), strerror(errno));
            if(errno == ENOENT)
                // If the file not exist, return 404
                return ERR_NOT_FOUND;
            else
                return ERR_INTERNAL_SERVER_ERR;
        }
        // Read file
        void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, file_fd, 0);
        close(file_fd);
        // ERROR process
        if(addr == MAP_FAILED)
        {
            WARN("Can not map file [%s] -> mem! (%s)", path_.c_str(), strerror(errno));
            return ERR_INTERNAL_SERVER_ERR;
        }
        // Save the data from memory in responseBody
        char* file_data_ptr = static_cast<char*>(addr);
        string responseBody(file_data_ptr, file_data_ptr + st.st_size);
        // clear the memory
        int res = munmap(addr, st.st_size);
        if(res == -1)
            WARN("Can not unmap file [%s] -> mem! (%s)", path_.c_str(), strerror(errno));
        // get the content type
        string suffix = path_;
        // find the .
        size_t dot_pos;
        while((dot_pos = suffix.find('.')) != string::npos)
            suffix = suffix.substr(dot_pos + 1);

        return sendResponse("200", "OK", MimeType::getMineType(suffix), responseBody);
    }

    // For POST, the http body is passed into the target executable file and the result is returned to the client
    else if(method_ == METHOD_POST)
    {
        // create two pipes
        int cgi_output[2];
        int cgi_input[2];

        if (pipe2(cgi_output, O_CLOEXEC) == -1) {
            WARN("cgi_output create error. (%s)", strerror(errno));
            return ERR_INTERNAL_SERVER_ERR;
        }
        if (pipe2(cgi_input, O_CLOEXEC) == -1) {
            WARN("cgi_input create error. (%s)", strerror(errno));
            close(cgi_output[0]);
            close(cgi_output[1]);
            return ERR_INTERNAL_SERVER_ERR;
        }

        pid_t pid;
        if((pid = fork()) < 0)
        {
            WARN("Fork error. (%s)", strerror(errno));
            close(cgi_input[0]);
            close(cgi_input[1]);
            close(cgi_output[0]);
            close(cgi_output[1]);
            return ERR_INTERNAL_SERVER_ERR;
        }

        if(pid == 0)
        {
            if(setpgid(0, 0) == -1)
                FATAL("setpgid fail in child process! (%s)", strerror(errno));
            if(prctl(PR_SET_PDEATHSIG, SIGKILL) == -1)
                FATAL("prctl fail in child process! (%s)", strerror(errno));

            if(dup2(cgi_input[0], 0) == -1
               || dup2(cgi_output[1], 1) == -1
               || dup2(1, 2) == -1)
                FATAL("dup2 fail! (%s)", strerror(errno));
            close(cgi_input[0]);
            close(cgi_input[1]);
            close(cgi_output[0]);
            close(cgi_output[1]);

            // read the data
            char path[path_.size() + 1];
            strcpy(path, path_.c_str());
            char* const args[] = { path, NULL };

            // execute the program
            execve(path, args, environ);
            FATAL("execve fail in child process! (%s)", strerror(errno));
        }
        else
        {
            close(cgi_input[0]);
            close(cgi_output[1]);

            ssize_t len = writen(cgi_input[1], http_body_.c_str(), http_body_.length(), true);
            if(len <= 0)
                WARN("Write %ld bytes to CGI input fail! (%s)", http_body_.length(), strerror(errno));

            close(cgi_input[1]);

            int timeouts = maxCGIRuntime;
            while(true)
            {
                if(!usleep(cgiStepTime * 1000))
                    timeouts -= cgiStepTime;
                int wstats = -1;
                // wait the child finish his job
                int waitpid_ret = waitpid(pid, &wstats, WNOHANG);
                if(waitpid_ret < 0)
                {
                    WARN("waitpid error. (%s)", strerror(errno));
                    // you should turn the output pipe off
                    close(cgi_output[0]);
                    return ERR_INTERNAL_SERVER_ERR;
                }
                else if(waitpid_ret > 0) {
                    bool ifExited = WIFEXITED(wstats);
                    bool ifKilled = WIFSIGNALED(wstats) && (WTERMSIG(wstats) != 0);
                    if (ifExited || ifKilled)
                        break;
                }
                else if(timeouts <= 0)
                {
                    /**
                     * @brief 把 kill 放到循环内部是为了 waitpid 回收子进程
                     * NOTE: -pid 指的是杀死当前子进程以及该子进程自身的子进程,例如shell脚本
                     * NOTE: 再kill一次 pid 是为了防止子进程太久没有轮到执行,仍然处于fork与execl之间的状态
                     *       此时,之前的 kill -pid 将会不起作用.因此为了确保子进程一定被kill,需要再kill一次pid
                     */
                    int res_kill_sub = kill(pid, SIGKILL);
                    int res_kill_pgid = 0;
                    // Kill * -pid * only after the pgid of the child process changes to prevent the child process in other threads from being injured by mistake
                    if(getpgid(pid) == pid)
                        res_kill_pgid = kill(-pid, SIGKILL);
                    assert(!res_kill_sub && !res_kill_pgid);
                    WARN("Sub process timeout.");
                }
            }

            string responseBody;
            char buf[MAXBUF];

            if(!setFdNoBlock(cgi_output[0]))
            {
                WARN("set fd(%d) no block fail! (%s)", cgi_output[0], strerror(errno));
                close(cgi_output[0]);
                return ERR_INTERNAL_SERVER_ERR;
            }
            while((len = readn(cgi_output[0], buf, MAXBUF)) > 0)
                responseBody += string(buf, buf + len);
            close(cgi_output[0]);

            if(responseBody.empty())
                return ERR_INTERNAL_SERVER_ERR;
            // send the data
            return sendResponse("200", "OK", MimeType::getMineType("txt"), responseBody);
        }
    }
    else
        return ERR_INTERNAL_SERVER_ERR;
    UNREACHABLE();
    return ERR_SUCCESS;
}

bool HttpHandler::handleErrorType(HttpHandler::ERROR_TYPE err)
{
    bool isSuccess = false;
    switch(err)
    {
        case ERR_SUCCESS:
            isSuccess = true;
            break;
        case ERR_READ_REQUEST_FAIL:
            ERROR("HTTP Read request failed ! (%s)", strerror(errno));
            state_ = STATE_FATAL_ERROR;
            break;
        case ERR_AGAIN:
            --againTimes_;
            INFO("HTTP waiting for more messages...");
            if(againTimes_ <= 0)
            {
                state_ = STATE_FATAL_ERROR;
                WARN("Reach max read times");
            }
            break;
        case ERR_CONNECTION_CLOSED:
            INFO("HTTP Socket(%d) was closed.", client_fd_);
            state_ = STATE_FATAL_ERROR;
            break;
        case ERR_SEND_RESPONSE_FAIL:
            ERROR("Send Response failed !");
            state_ = STATE_FATAL_ERROR;
            break;
        case ERR_BAD_REQUEST:
            WARN("HTTP Bad Request.");
            sendErrorResponse("400", "Bad Request");
            state_ = STATE_ERROR;
            break;
        case ERR_NOT_FOUND:
            WARN("HTTP Not Found.");
            sendErrorResponse("404", "Not Found");
            state_ = STATE_ERROR;
            break;
        case ERR_LENGTH_REQUIRED:
            WARN("HTTP Length Required.");
            sendErrorResponse("411", "Length Required");
            state_ = STATE_ERROR;
            break;
        case ERR_NOT_IMPLEMENTED:
            WARN("HTTP Request method is not implemented.");
            sendErrorResponse("501", "Not Implemented");
            state_ = STATE_ERROR;
            break;
        case ERR_INTERNAL_SERVER_ERR:
            WARN("HTTP Internal Server Error.");
            sendErrorResponse("500", "Internal Server Error");
            state_ = STATE_ERROR;
            break;
        case ERR_HTTP_VERSION_NOT_SUPPORTED:
            WARN("HTTP Request HTTP Version Not Supported.");
            sendErrorResponse("505", "HTTP Version Not Supported");
            state_ = STATE_ERROR;
            break;
        default:
            UNREACHABLE();
    }
    return isSuccess;
}

HttpHandler::ERROR_TYPE HttpHandler::sendResponse(const string& responseCode, const string& responseMsg,
                                                  const string& responseBodyType, const string& responseBody)
{
    stringstream sstream;
    sstream << "HTTP/1.1" << " " << responseCode << " " << responseMsg << "\r\n";
    sstream << "Connection: " << (isKeepAlive_ ? "Keep-Alive" : "Close") << "\r\n";
    if(isKeepAlive_)
        sstream << "Keep-Alive: timeout=" << timeoutPerRequest << ", max=" << againTimes_ << "\r\n";

    sstream << "Server: WebServer/1.1" << "\r\n";
    sstream << "Content-length: " << responseBody.size() << "\r\n";
    sstream << "Content-type: " << responseBodyType << "\r\n";
    sstream << "\r\n";
    // if request is HEAD, do not send the http body
    if(method_ != METHOD_HEAD)
        sstream << responseBody;

    string&& response = sstream.str();

    ssize_t len = writen(client_fd_, (void*)response.c_str(), response.size());

    // output the response data
    INFO("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<- Response Packet ->>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ");
    INFO("{%s}", escapeStr(response, MAXBUF).c_str());

    if(len < 0 || static_cast<size_t>(len) != response.size())
        return ERR_SEND_RESPONSE_FAIL;
    return ERR_SUCCESS;
}

HttpHandler::ERROR_TYPE HttpHandler::sendErrorResponse(const string& errCode, const string& errMsg)
{
    string errStr = errCode + " " + errMsg;
    string responseBody =
            "<html>"
            "<title>" + errStr + "</title>"
                                 "<body>" + errStr +
            "<hr><em> Kelpie Web Server</em>"
            "</body>"
            "</html>";
    return sendResponse(errCode, errMsg, "text/html", responseBody);
}

bool HttpHandler::RunEventLoop()
{
    if(!handleErrorType(readRequest()))
        return false;

    // parse the info ------------------------------------------
    // 1. parse first line
    if(state_ == STATE_PARSE_URI && handleErrorType(parseURI()))
        state_ = STATE_PARSE_HEADER;
    // 2. parse each header
    if(state_ == STATE_PARSE_HEADER && handleErrorType(parseHttpHeader()))
        state_ = STATE_PARSE_BODY;
    // 3. parse the http body
    if(state_ == STATE_PARSE_BODY)
    {
        if(method_ != METHOD_POST || handleErrorType(parseBody()))
            state_ = STATE_ANALYSI_REQUEST;
    }
    // 4. process data
    if(state_ == STATE_ANALYSI_REQUEST && handleErrorType(handleRequest()))
        state_ = STATE_FINISHED;

    if(state_ == STATE_ERROR || state_ == STATE_FINISHED)
    {
        if(isKeepAlive_)
            reset();
        else
            return false;
    }
    else if(state_ == STATE_FATAL_ERROR)
        return false;

    // if run here that means to need more data
    bool ret1 = true;
    if(timer_)
        ret1 = epoll_->modify(timer_->getFd(), getTimerEpollEvent(), getTimerTriggerCond());
    bool ret2 = epoll_->modify(client_fd_, getClientEpollEvent(), getClientTriggerCond());
    assert(ret1 && ret2);

    return true;
}