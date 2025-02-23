#pragma once


#include <cstring>
#define SYSTEM_MALLOC(size) malloc(size)
#define SYSTEM_FREE(ptr) free(ptr)
#define ME_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define ME_MEMMOVE(dst, src, size) memmove(dst, src, size)
#define ME_MEMCLEAR(dst, size) memset(dst, 0, size)