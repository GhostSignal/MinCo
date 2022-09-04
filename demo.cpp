#include <iostream>
#include <stdexcept>
#include <mswsock.h>
#include "MinCo/eventloop.hpp"

using MinCo::Co;
using namespace std::chrono_literals;

static const WSADATA* const __wsa_data__ = []() {
    static WSADATA __wsa_data__;
    int err = WSAStartup(0x0202, &__wsa_data__);
    if (err) {
        throw std::runtime_error("WSAStartup() error: " + std::to_string(err));
    }
    return &__wsa_data__;
}();

static const HANDLE __iocp__ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

void MinCo::BaseEventLoop::io_handle()
{
    DWORD recvd;
    size_t comp_key;
    OVERLAPPED* ovl = NULL;
    while (GetQueuedCompletionStatus(__iocp__, &recvd, &comp_key, &ovl, 0)) {
        (*(std::coroutine_handle<>*)++ovl).resume();
    }
}

static MinCo::BaseEventLoop __loop__;

struct CoSock {
    CoSock(HANDLE iocp, int family = AF_INET, int type = SOCK_STREAM, int proro = IPPROTO_TCP)
    {
        _sock = WSASocket(family, type, proro, 0, 0, WSA_FLAG_OVERLAPPED);
        if (!CreateIoCompletionPort((HANDLE)_sock, iocp, (size_t)this, 0)) {
            throw std::runtime_error("CreateIoCompletionPort() error: " + std::to_string(WSAGetLastError()));
        }
    }
    ~CoSock()
    {
        closesocket(_sock);
    }

    Co<void> bind_listen(const char* ipv4, unsigned short port, int backlog)
    {
        sockaddr_in addr{
            .sin_family = AF_INET, .sin_port = htons(port), .sin_addr = {.S_un = {.S_addr = inet_addr(ipv4)}}};
        if (bind(_sock, (sockaddr*)&addr, sizeof(addr))) {
            throw std::runtime_error("bind() error: " + std::to_string(WSAGetLastError()));
        }
        if (listen(_sock, backlog)) {
            throw std::runtime_error("listen() error: " + std::to_string(WSAGetLastError()));
        }
        std::cout << "Listening on: " << ipv4 << ":" << port << "\n";  // DEBUG
        co_return;
    }

    Co<void> accept(CoSock& accepter, std::chrono::seconds wait_timeout = INFINITE * 1s)
    {
        co_await prepare;
        union {
            sockaddr_in addr_buffer[4];
            DWORD dword_buffer[];
        };
        if (!AcceptEx(_sock, accepter._sock, addr_buffer, 0, 32, 32, dword_buffer, &overlapped)) {
            int err = WSAGetLastError();
            std::cout << "AcceptEX err = " << err << std::endl;
            if (err == WSA_IO_PENDING) {
                // CancelIoEx((HANDLE)_sock, &overlapped);
                timeout._tm = std::chrono::steady_clock::now() + wait_timeout;
                co_await timeout;
                if (!WSAGetOverlappedResult(_sock, &overlapped, dword_buffer, false, dword_buffer + 1)) {
                    throw std::runtime_error("AcceptEx() end error: " + std::to_string(WSAGetLastError()));
                };
            } else {
                throw std::runtime_error("AcceptEx() pre error: " + std::to_string(err));
            }
        };
        std::cout << dword_buffer[0] << ' ' << dword_buffer[1] << std::endl;
        std::cout << inet_ntoa(addr_buffer[2].sin_addr) << ":";
        std::cout << ntohs(addr_buffer[2].sin_port) << std::endl;
    }

    Co<DWORD> recv(WSABUF&& wsa_buf, std::chrono::seconds wait_timeout = INFINITE * 1s)
    {
        co_await prepare;
        DWORD bytes, flags = 0;
        if (WSARecv(_sock, &wsa_buf, 1, &bytes, &flags, &overlapped, NULL) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cout << "WSARecv err = " << err << std::endl;
            if (err == WSA_IO_PENDING) {
                timeout._tm = std::chrono::steady_clock::now() + wait_timeout;
                co_await timeout;
                if (!WSAGetOverlappedResult(_sock, &overlapped, &bytes, false, &flags)) {
                    throw std::runtime_error("WSARecv() error: " + std::to_string(WSAGetLastError()));
                };
            } else {
                throw std::runtime_error("WSARecv() error: " + std::to_string(err));
            }
        };
        co_return bytes;
    }

    Co<DWORD> send(WSABUF&& wsa_buf, std::chrono::seconds wait_timeout = INFINITE * 1s)
    {
        co_await prepare;
        DWORD bytes, flags = 0;
        if (WSASend(_sock, &wsa_buf, 1, &bytes, flags, &overlapped, NULL) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            std::cout << "WSASend err = " << err << std::endl;
            if (err == WSA_IO_PENDING) {
                timeout._tm = std::chrono::steady_clock::now() + wait_timeout;
                co_await timeout;
                if (!WSAGetOverlappedResult(_sock, &overlapped, &bytes, false, &flags)) {
                    throw std::runtime_error("WSASend() error: " + std::to_string(WSAGetLastError()));
                };
            } else {
                throw std::runtime_error("WSASend() error: " + std::to_string(err));
            }
        }
        co_return bytes;
    }

    union {
        struct {
            SOCKET _sock;
            OVERLAPPED overlapped;
        };
        struct {
            SOCKET _sock;
            OVERLAPPED _ov;
            std::coroutine_handle<> _co;
            constexpr bool await_ready() const noexcept
            {
                return false;
            }
            bool await_suspend(std::coroutine_handle<> co) noexcept
            {
                _ov = {};
                _co = co;
                return false;
            }
            constexpr void await_resume() const noexcept
            {}
        } prepare;
        struct {
            SOCKET _sock;
            OVERLAPPED _ov;
            std::coroutine_handle<> _co;
            std::chrono::steady_clock::time_point _tm;
            bool await_ready() const noexcept
            {
                return false;
            }
            void await_suspend(std::coroutine_handle<>) const noexcept
            {
                __loop__.timerQueue.insert({_tm, _co});
            }
            void await_resume()
            {
                if (CancelIoEx((HANDLE)_sock, &_ov)) {
                    // timeout
                    throw std::runtime_error("CoSock.timeout error");
                } else {
                    // overlap
                    auto i = __loop__.timerQueue.find(_tm);
                    while (i->second != _co) {
                        ++i;
                    }
                    __loop__.timerQueue.erase(i);
                };
            }
        } timeout;
    };
};

Co<void> echo_once()
{
    CoSock a(__iocp__), b(__iocp__);
    co_await a.bind_listen("0.0.0.0", 50001, 5);
    std::cout << "listening...\n";
    co_await a.accept(b);
    char buffer[256] = "HTTP/1.0 200 OK\r\n\r\n";
    co_await b.recv({256 - 19, buffer + 19});
    co_await b.send({(u_long)strlen(buffer), buffer});
}

int main()
{
    __loop__.run_until_complete(echo_once());
}