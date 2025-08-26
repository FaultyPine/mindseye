#pragma once

#include "core/me_defines.h"

#include <functional>

struct ScopeExit 
{
    std::function<void()> f;
    ScopeExit(std::function<void()> func) : f(std::move(func)) {}
    ~ScopeExit() { f(); }
};
#define ME_ON_SCOPE_EXIT(...) ScopeExit ME_MACRO_CONCAT(onScopeExit_,__LINE__)((__VA_ARGS__));

