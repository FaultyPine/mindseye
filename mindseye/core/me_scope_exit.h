#pragma once


#include <functional> // :/

struct ScopeExit 
{
    std::function<void()> f;
    ScopeExit(std::function<void()> func) : f(std::move(func)) {}
    ~ScopeExit() { f(); }
};
#define ON_SCOPE_EXIT(func) ScopeExit ME_MACRO_CONCAT(onScopeExit_,__LINE__)(func);
