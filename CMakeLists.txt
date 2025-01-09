cmake_minimum_required(VERSION 3.24)
project(packaged_task)

set(CMAKE_CXX_STANDARD 11)


add_executable(packaged_task
        main.cpp
        packaged_task.h
        coroutine.h)
