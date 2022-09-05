
#ifndef __MinCo__HPP__eventloop__
#define __MinCo__HPP__eventloop__

#include "co_wraper.hpp"
#include <chrono>
#include <deque>
#include <map>

namespace MinCo {

struct BaseEventLoop {  // 仅支持单线程
    using time_point = std::chrono::steady_clock::time_point;
    std::multimap<time_point, std::coroutine_handle<>> timerQueue = {{time_point::max(), 0}};
    std::deque<std::coroutine_handle<>> readyQueue;

    time_point now()
    {
        return std::chrono::steady_clock::now();
    }

    void __timer_add(time_point time, std::coroutine_handle<> handle)
    {
        timerQueue.insert({time, handle});
    }

    void __timer_del(time_point time, std::coroutine_handle<> handle)
    {
        auto iter = timerQueue.find(time);
        while (iter->second != handle) {
            ++iter;
        }
        timerQueue.erase(iter);
    }

    time_point __timer_handle()
    {
        // timer
        auto iter = timerQueue.begin(), begin = iter;
        time_point now = std::chrono::steady_clock::now();
        for (; iter->first <= now; ++iter) {
            readyQueue.push_back(iter->second);
        }
        timerQueue.erase(begin, iter);

        // ready
        for (auto iter = readyQueue.begin(); iter != readyQueue.end(); ++iter) {
            iter->resume();
        }
        readyQueue.clear();

        return iter->first;
    }

    void __event_handle(time_point nxt);

    void run_forever()
    {
        for (;;) {
            __event_handle(__timer_handle());
        }
    }

    template <class Tp>
    decltype(auto) run_until_complete(Co<Tp>&& coro)
    {
        auto handle = coro.await_suspend(std::noop_coroutine());
        readyQueue.push_back(handle);
        while (!handle.done()) {
            __event_handle(__timer_handle());
        }
        return coro.await_resume();
    }

    template <class Tp>
    void detach_coro(Co<Tp>&& coro)
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        readyQueue.push_back(coro);
#pragma clang diagnostic pop
    }
};

}  // namespace MinCo

#endif
