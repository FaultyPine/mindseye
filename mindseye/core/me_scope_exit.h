#pragma once

#include "core/me_defines.h"

#include <functional>
#include <utility>

struct ScopeExit 
{
    std::function<void()> f;
    ScopeExit(std::function<void()> func) : f(std::move(func)) {}
    ~ScopeExit() { f(); }
};

#define ME_ON_SCOPE_EXIT(...) ScopeExit ME_MACRO_CONCAT_EX(onScopeExit_, __LINE__)((__VA_ARGS__));

