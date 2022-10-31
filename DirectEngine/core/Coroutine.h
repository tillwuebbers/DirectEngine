#pragma once

#include <coroutine>

struct CoroutineReturn {
    struct promise_type {
        template<class... Types>
        void* operator new(std::size_t size, MemoryArena& arena, Types... args)
        {
            return arena.Allocate(size);
        }
        void operator delete(void* ptr)
        {
            // We allocated in an arena, no need to do anything
            return;
        }
        CoroutineReturn get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

struct CoroutineAwaiter {
    std::coroutine_handle<>* handle;
    constexpr bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) { *handle = h; }
    constexpr void await_resume() const noexcept {}
};
