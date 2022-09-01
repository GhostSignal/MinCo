#ifndef __MinCo__HPP__eventloop__
#define __MinCo__HPP__eventloop__

#include "co_wraper.hpp"

#include <deque>
#include <map>
#include <chrono>

namespace MinCo {

using namespace std::chrono_literals;

struct DemoEventLoop {
    template <class Tp>
    void detach_coro(Co<Tp>&& coro)
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        readyQueue.push_back(coro);
#pragma clang diagnostic pop
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
        std::coroutine_handle<> handle = coro.await_suspend(std::noop_coroutine());
        readyQueue.push_back(handle);
        while (!handle.done()) {
            __run_once();
        }
        return coro.await_resume();
    }

    auto sleep(std::chrono::milliseconds delay)
    {
        struct _ {
            std::multimap<std::chrono::time_point<std::chrono::steady_clock>, std::coroutine_handle<>>* timer;
            std::chrono::time_point<std::chrono::steady_clock> time;

            constexpr bool await_ready() const noexcept
            {
                return false;
            }
            void await_suspend(std::coroutine_handle<> handle)
            {
                timer->insert({time, handle});
            }
            void await_resume()
            {}
        };
        return _{&timerQueue, std::chrono::steady_clock::now() + delay};
    }

private:
    std::deque<std::coroutine_handle<>> readyQueue;
    std::multimap<std::chrono::time_point<std::chrono::steady_clock>, std::coroutine_handle<>> timerQueue;

    void __run_once()
    {
        while (!readyQueue.empty()) {
            readyQueue.front().resume();
            readyQueue.pop_front();
        }
        while (!timerQueue.empty() && timerQueue.begin()->first <= std::chrono::steady_clock::now()) {
            readyQueue.push_back(timerQueue.begin()->second);
            timerQueue.erase(timerQueue.begin());
        }
    }
};

}  // namespace MinCo

#endif
