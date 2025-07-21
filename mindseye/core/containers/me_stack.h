#pragma once

#include "me_hybrid_array.h"

template<typename T, int FIXED_SIZE = 100>
struct Stack 
{
    HybridArray<T, FIXED_SIZE> data;

    Stack() = default;

    void push(const T& value) 
    {
        data.push_back(value);
    }

    T pop() 
    {
        ME_ASSERT(!empty());
        return data.pop();
    }

    T& top() 
    {
        ME_ASSERT(!empty());
        return data.at(data.size - 1);
    }

    const T& top() const 
    {
        ME_ASSERT(!empty());
        return data.at(data.size - 1);
    }

    bool empty() const 
    {
        return data.size == 0;
    }

    u32 size() const 
    {
        return static_cast<u32>(data.size);
    }
};