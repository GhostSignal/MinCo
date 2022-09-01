#ifndef __MinCo__HPP__co_wraper__
#define __MinCo__HPP__co_wraper__

#include <coroutine>
#include <exception>
#include <variant>

namespace MinCo {

struct chain_awaiter {
    std::coroutine_handle<> parent;
    bool await_ready() const noexcept
    {
        return !parent;
    }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept
    {
        return parent;
    }
    constexpr void await_resume() const noexcept
    {}
};

template <class Tp>
struct chain_promise {
#define __MinCo__TMP__chain_promise__                              \
    chain_awaiter _chain;                                          \
    chain_promise* get_return_object() noexcept                    \
    {                                                              \
        return this;                                               \
    }                                                              \
    constexpr std::suspend_always initial_suspend() const noexcept \
    {                                                              \
        return {};                                                 \
    }                                                              \
    void unhandled_exception() noexcept                            \
    {                                                              \
        result = std::current_exception();                         \
    }                                                              \
    chain_awaiter final_suspend() const noexcept                   \
    {                                                              \
        return _chain;                                             \
    }
    __MinCo__TMP__chain_promise__;

    std::variant<std::exception_ptr, Tp> result;
    chain_awaiter yield_value(const Tp& value) noexcept
    {
        result = value;
        return _chain;
    }
    void return_value(const Tp& value) noexcept
    {
        result = std::move(value);
    }
};

template <>
struct chain_promise<void> {
    __MinCo__TMP__chain_promise__;
#undef __MinCo__TMP__chain_promise__

    std::exception_ptr result;
    constexpr void return_void() const noexcept
    {}
};

template <class Tp>
struct Co {
    // coroutine interface
    using promise_type = chain_promise<Tp>;
    Co(promise_type* p) noexcept : handle(std::coroutine_handle<promise_type>::from_promise(*p))
    {}

    // awaitable interface
    constexpr bool await_ready() const noexcept
    {
        return false;
    }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) const noexcept
    {
        handle.promise()._chain.parent = parent;
        return handle;
    }
    decltype(auto) await_resume()
    {
        if (handle.promise().result.index()) {
            return std::move(std::get<Tp>(handle.promise().result));
        } else {
            std::rethrow_exception(std::get<std::exception_ptr>(handle.promise().result));
        }
    };

    // resource management
    Co(Co&) = delete;
    Co& operator=(Co&) = delete;
    Co(Co&& moved) noexcept : handle(moved.handle)
    {
        moved.handle = 0;
    }
    [[deprecated]] operator std::coroutine_handle<>()
    {
        auto tmp = handle;
        handle = 0;
        return tmp;
    }
    ~Co() noexcept
    {
        if (handle) {
            handle.destroy();
        }
    }

private:
    std::coroutine_handle<promise_type> handle;
};

template <>
inline decltype(auto) Co<void>::await_resume()
{
    if (handle.promise().result) {
        std::rethrow_exception(handle.promise().result);
    }
}

}  // namespace MinCo

#endif
