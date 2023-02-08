//
// Created by kelpie on 2/3/23.
//

#ifndef WEBSERVER_LOG_H
#define WEBSERVER_LOG_H

#include <cstdio>
#include <unistd.h>
#include <sys/syscall.h>

#include "MutexLock.h"

// Terminal colorful text code
#define cRST "\x1b[0m"      // Red
#define cLRD "\x1b[1;91m"   // Reset
#define cYEL "\x1b[1;93m"   // Yellow
#define cBRI "\x1b[1;97m"   // White && Bold
#define cLBL "\x1b[1;94m"   // Blue

// GLOBAL output LOCK
extern  MutexLock global_log_lock;

#if true
#define INFO(x...) do { \
    MutexLockGuard log_guard(global_log_lock); \
    fprintf(stdout, cLBL "[*]" cRST x);        \
    fprintf(stdout, cRST "\n");                \
    fflush(stdout);     \
}while(false)

#define WARN(x...) do { \
    MutexLockGuard log_guard(global_log_lock);    \
    fprintf(stderr, "(Thread %lx): ", syscall(SYS_gettid));  \
    fprintf(stderr, cYEL "[!] " cBRI "WARNING: " cRST x); \
    fprintf(stderr, cRST "\n");    \
    fflush(stderr);    \
  } while (0)

#define ERROR(x...) do { \
    MutexLockGuard log_guard(global_log_lock);    \
    fprintf(stderr, "(Thread %lx): ", syscall(SYS_gettid));  \
    fprintf(stderr, cLRD "[-] " cRST x); \
    fprintf(stderr, cRST "\n"); \
    fflush(stderr);    \
  } while (0)

#define FATAL(x...) do { \
    MutexLockGuard log_guard(global_log_lock);                  \
    fprintf(stderr, "(Thread %lx): ", syscall(SYS_gettid));     \
    fprintf(stderr, cRST cLRD "[-] PROGRAM ABORT : " cBRI x);   \
    fprintf(stderr, cLRD "\n         Location : " cRST "%s(), %s:%u\n\n", \
         __FUNCTION__, __FILE__, __LINE__); \
    fflush(stderr);    \
    abort(); \
  } while (0)

#else


#endif
#define UNREACHABLE(x) FATAL("UNREACHABLE CODE");

#endif //WEBSERVER_LOG_H
