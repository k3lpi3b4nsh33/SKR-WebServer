cmake_minimum_required(VERSION 3.24)
project(webserver1)

set(CMAKE_CXX_STANDARD 11)


add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=1)
add_executable(WebServer main.cpp epoll.h Utils.h Utils.cpp Log.h Log.cpp MutexLock.h epoll.cpp Condition.h ThreadPool.cpp ThreadPool.h Timer.cpp Timer.h HttpHandler.cpp HttpHandler.h)

