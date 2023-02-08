//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_HTTPHANDLER_H
#define WEBSERVER_HTTPHANDLER_H

#include <iostream>
#include <map>

#include "epoll.h"
#include "Timer.h"

using namespace std;

class HttpHandler
{
public:
    explicit HttpHandler(Epoll* epoll, int client_fd, Timer* timer);
    ~HttpHandler();

    bool RunEventLoop();
    int getClientFd() { return client_fd_; }
    Epoll* getEpoll() { return epoll_;}
    Timer* getTimer() {return timer_;}

    int getClientTriggerCond() { return EPOLLET | EPOLLIN | EPOLLONESHOT | EPOLLRDHUP | EPOLLHUP; }
    int getTimerTriggerCond()  { return EPOLLET | EPOLLIN | EPOLLONESHOT; };

    void* getClientEpollEvent() { return &client_event_; }
    void* getTimerEpollEvent()  { return &timer_event_;}

    static void setWWWPath(string path) { www_path = path; };
    static string getWWWPath()          { return www_path; }

    enum STATE_TYPE
    {
        STATE_PARSE_URI,
        STATE_PARSE_HEADER,
        STATE_PARSE_BODY,
        STATE_ANALYSI_REQUEST,
        STATE_FINISHED,
        STATE_ERROR,
        STATE_FATAL_ERROR
    };
    STATE_TYPE getState() { return state_; }

private:
    enum ERROR_TYPE{
        ERR_SUCCESS = 0,

        ERR_READ_REQUEST_FAIL,
        ERR_AGAIN,
        ERR_CONNECTION_CLOSED,

        ERR_SEND_RESPONSE_FAIL,

        ERR_BAD_REQUEST,                //  400 Bad Request
        ERR_NOT_FOUND,                  //  404 Not Found
        ERR_LENGTH_REQUIRED,            //  411 Length Required

        ERR_NOT_IMPLEMENTED,            //  501 Not Implemented
        ERR_INTERNAL_SERVER_ERR,        //  500 Internal Server Error
        ERR_HTTP_VERSION_NOT_SUPPORTED  //  505 HTTP Version Not Supported
    };
    enum HTTP_VERSION{
        HTTP_1_0,           // HTTP/1.0
        HTTP_1_1,           // HTTP/1.1
    };


    enum METHOD_TYPE {
        METHOD_GET,         // GET
        METHOD_POST,        // POST
        METHOD_HEAD
    };


    static string www_path;


    const size_t MAXBUF = 1024;
    const int maxAgainTimes = 10;
    const int maxCGIRuntime = 1000;
    const int cgiStepTime = 1;
    const int timeoutPerRequest = 10;

    int client_fd_;
    EpollEvent client_event_;

    Timer* timer_;
    EpollEvent timer_event_;;

    Epoll* epoll_;

    string request_;
    map<string, string> headers_;
    METHOD_TYPE method_;
    string path_;
    HTTP_VERSION http_version_;
    STATE_TYPE state_;

    int againTimes_;
    string http_body_;
    bool isKeepAlive_;

    size_t curr_parse_pos_;
    void reset();

    ERROR_TYPE readRequest();
    ERROR_TYPE parseURI();
    ERROR_TYPE parseHttpHeader();
    ERROR_TYPE parseBody();
    ERROR_TYPE handleRequest();
    bool handleErrorType(ERROR_TYPE err);

    ERROR_TYPE sendResponse(const string& responseCode, const string& responseMsg,  const string& responseBodyType, const string& responseBody);
    ERROR_TYPE sendErrorResponse(const string& errCode, const string& errMsg);
};

class MimeType
{
private:
    map<string, string> mime_map_;
    string getMineType_(string suffix)
    {
        if(mime_map_.find(suffix) != mime_map_.end())
            return mime_map_[suffix];
        else
            return mime_map_["default"];
    }
public:
    MimeType()
    {
        mime_map_["doc"] = "application/msword";
        mime_map_["gz"] = "application/x-gzip";
        mime_map_["ico"] = "application/x-ico";

        mime_map_["gif"] = "image/gif";
        mime_map_["jpg"] = "image/jpeg";
        mime_map_["png"] = "image/png";
        mime_map_["bmp"] = "image/bmp";

        mime_map_["mp3"] = "audio/mp3";
        mime_map_["avi"] = "video/x-msvideo";

        mime_map_["html"] = "text/html";
        mime_map_["htm"] = "text/html";
        mime_map_["css"] = "text/html";
        mime_map_["js"] = "text/html";

        mime_map_["c"] = "text/plain";
        mime_map_["txt"] = "text/plain";
        mime_map_["default"] = "text/plain";
    }

    static string getMineType(string suffix)
    {
        static MimeType _mimeTy;
        return _mimeTy.getMineType_(suffix);
    }
};

#endif //WEBSERVER_HTTPHANDLER_H
