#include <iostream>
#include <cstdio>
#include <numeric>
#include <vector>
#include <thread>
#include <chrono>
#include "coroutine.h"
#include "packaged_task.h"
using namespace std;

int add(int a, int b)
{
    printf("sleep for 3s\n");
    st_usleep(3*1000*1000);
    printf("sleep done\n");
    return a+b;
    //throw std::logic_error("exception test");
}

void test_task()
{
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

    cout << "test task done" << endl;
}


void accumulate_func(std::vector<int>::iterator first,
                std::vector<int>::iterator last,
                Promise<int> &accumulate_promise)
{
    st_usleep(500*1000);
    int sum = std::accumulate(first, last, 0);
    accumulate_promise.set_value(sum); // Notify future
}

void test_promise()
{
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6};
    Promise<int> accumulate_promise;
    Future<int> accumulate_future = accumulate_promise.get_future();
    Coroutine cor(accumulate_func, numbers.begin(), numbers.end(),
                            std::ref(accumulate_promise));
    std::cout<< "accumulate_futures=" << accumulate_future.get() << std::endl;
    cor.join();
}

void test_promise1()
{
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6};
    Promise<int> accumulate_promise;
    Future<int> accumulate_future = accumulate_promise.get_future();
    Coroutine cor(accumulate_func, numbers.begin(), numbers.end(),
                  std::ref(accumulate_promise));

    time_point tp1 = chrono::steady_clock::now();
    FutureStatus stauts = accumulate_future.wait_for(chrono::seconds(1));

    std::cout << "time duration=" << chrono::duration_cast<chrono::duration<int, milli>>(
            chrono::steady_clock::now() - tp1).count() << "ms" << endl;

    if (stauts == FutureStatus::Ready) {
        std::cout<< "accumulate_futures=" << accumulate_future.get() << std::endl;
    } else {
        std::cout<< "future timeout" << std::endl;
    }
    cor.join();
}


int main() {
    if (st_init() < 0) {
        printf("st_init failed");
        return -1;
    }

    printf("init done\n");

    test_task();
    test_promise();
    test_promise1();
    st_thread_exit(NULL);
    return 0;
}
