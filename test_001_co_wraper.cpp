#include "MinCo/co_wraper.hpp"

#include <iostream>

using namespace MinCo;

Co<int> fib()
{
    for (int a = 1, b = 1;;) {
        co_yield a;
        a += b;
        co_yield b;
        b += a;
    }
}

Co<const char*> err()
{
    throw "err1() throw\n";
    co_return "err1() return\n";
}

Co<int> task()
{
    try {
        std::cout << co_await err();
    } catch (const char* exc) {
        std::cout << exc;
    }
    int sum = 0;
    auto gen = fib();
    for (int tmp; sum < 1000;) {
        tmp = co_await gen;
        std::cout << tmp << ' ';
        sum += tmp;
    }
    std::cout << std::endl;
    co_return sum;
}

int main()
{
    auto f = task();
    f.await_suspend(std::noop_coroutine()).resume();
    std::cout << "result = " << f.await_resume();
    return 0;
}