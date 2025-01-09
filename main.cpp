
#include <iostream>
#include "packaged_task.h"
#include <thread>

int add(int a, int b) {
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

    int a = ret.get();
    std::cout << "task=" << a << std::endl;
    return 0;
}
