#pragma once

#include "sched/thread.h"
#include <string_view>

namespace sched
{
auto create(std::string_view name, void (*fn)(void*), void* arg) -> Thread*;
auto yield() -> void;
auto block();
auto wake(Thread* t) -> void;
[[noreturn]] auto exit() -> void;
auto current() -> Thread*;
}
