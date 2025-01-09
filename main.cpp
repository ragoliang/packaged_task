#include <iostream>
#include <cstdio>
#include "coroutine.h"
#include "packaged_task.h"

int add(int a, int b)
{
    printf("sleep for 3s\n");
    st_usleep(3*1000*1000);
    printf("sleep done\n");
    throw std::logic_error("exception test");
}

int main() {
    if (st_init() < 0) {
        printf("st_init failed");
        return -1;
    }

    printf("init done\n");

    PackagedTask<int(int,int)> task1;

    PackagedTask<int(int,int)> task(add);

    task1 = std::move(task);

    Future<int> ret = task1.get_future();
    //task1(1,2);
    Coroutine cor(std::move(task1), 2, 10);
    printf("before join()\n");
    cor.join();
    printf("join()\n");

    try{
        int a = ret.get();
        std::cout << "task=" << a << std::endl;
    } catch(std::exception &e) {
        std::cout<< "exception="<< e.what()<< std::endl;
    }


    st_thread_exit(NULL);
    return 0;
}
