
#include <iostream>
#include "packaged_task.h"
#include <thread>

int add(int a, int b) {
    return a+b;
}

int add_throw(int a, int b) {
    throw std::logic_error("test throw exception");
    return a+b;
}

int main() {
    PackagedTask<int(int,int)> task1;

    PackagedTask<int(int,int)> task(add);

    task1 = std::move(task);

    Future<int>  ret =  task1.get_future();
    //task1(1,2);
    std::thread task_td(std::move(task1), 2, 10);
    task_td.join();
    std::cout << "task=" << ret.get() << std::endl;

    try {
        PackagedTask<int(int,int)> task2(add_throw);
        Future<int>  ret1 =  task2.get_future();
        task2(2,2);
        std::cout << "task2=" << ret1.get() << std::endl;
    } catch (std::exception &e) {
        std::cout << "task exception, e=" << e.what() << std::endl;
    }

    return 0;
}
