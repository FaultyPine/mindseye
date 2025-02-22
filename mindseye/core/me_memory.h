#pragma once


#include <cstring>
#define MEMCLEAR(dst, size) memset(dst, 0, size)
#define SYSTEM_MALLOC(size) malloc(size)