#pragma once

#include <cstdint>
#include <string_view>
namespace sched
{
enum class State : uint8_t
{
    Ready,
    Ruynnig,
    Blocked,
    Dead
};

struct Thread
{
    uint64_t rsp{};
    State state{State::Ready};
    uint32_t id{};
    std::string_view name;
    std::byte* stack{};
    std::size_t stack_size{};
    uint64_t wake_at{};
    Thread* next{};
};

extern "C" auto switch_context(uint64_t* save_rsp, uint64_t load_rsp) -> void;
extern "C" auto thread_trampoline() -> void;
extern "C" [[noreturn]] auto thread_start(void (*fn)(void*), void* arg) -> void;
}
