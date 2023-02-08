//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_UTILS_H
#define WEBSERVER_UTILS_H

#include <iostream>
#include <cstring>
#include <csignal>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::ostream;

int socket_bind_and_listen(int port);
bool setFdNoBlock(int fd);
bool setSocketNoDelay(int fd);

ssize_t readn(int fd, void* buf, size_t len);
ssize_t writen(int fd, const void* buf, size_t len, bool isWrite = false);

void handleSigpipe();
void printConnectionStatus(int client_fd_, string prefix);

string escapeStr(const string& str, size_t MAXBUF);

bool isNumericStr(string str);

size_t closeRemainingConnect(int listen_fd, int* idle_fd);
bool is_path_parent(const string& parent_path, const string& child_path);

#endif //WEBSERVER_UTILS_H
