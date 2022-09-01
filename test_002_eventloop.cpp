#include "MinCo/eventloop.hpp"

#include <iostream>
#include <thread>

using namespace MinCo;

DemoEventLoop loop[3];

Co<void> task()
{
    for (int i = 2; i >= 0; i--) {
        std::cout << std::this_thread::get_id() << std::endl;
        co_await loop[i].sleep(500ms);
    }
    std::cout << std::this_thread::get_id() << std::endl;
    std::cout << "fin" << std::endl;
}

int main()
{
    // 单线程/单协程时std::deque线程安全，多线程+多协程时std::deque不安全
    std::thread([]() { loop[1].run_forever(); }).detach();
    std::thread([]() { loop[2].run_forever(); }).detach();
    loop[0].run_until_complete(task());
}