
#ifndef __MinCo__HPP__eventloop__
#define __MinCo__HPP__eventloop__

#include "co_wraper.hpp"
#include <chrono>
#include <deque>
#include <map>

namespace MinCo {

struct BaseEventLoop {
    std::deque<std::coroutine_handle<>> readyQueue;
    std::multimap<std::chrono::steady_clock::time_point, std::coroutine_handle<>> timerQueue;
    void io_handle();

    void __run_once()
    {
        auto now = std::chrono::steady_clock::now();
        while (!timerQueue.empty() && timerQueue.begin()->first <= now) {
            readyQueue.push_back(timerQueue.begin()->second);
            timerQueue.erase(timerQueue.begin());
        }
        while (!readyQueue.empty()) {
            readyQueue.front().resume();
            readyQueue.pop_front();
        }
        io_handle();
    }

    void run_forever()
    {
        for (;;) {
            __run_once();
        }
    }

    template <class Tp>
    decltype(auto) run_until_complete(Co<Tp>&& coro)
    {
        auto handle = coro.await_suspend(std::noop_coroutine());
        readyQueue.push_back(handle);
        while (!handle.done()) {
            __run_once();
        }
        return coro.await_resume();
    }
};

}  // namespace MinCo

#endif
