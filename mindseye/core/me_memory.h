#pragma once


extern "C" void * malloc(size_t _Size);
extern "C" void  free(void *_Block);
extern "C" void * memcpy(void *_Dst, const void *_Src, size_t _Size);
extern "C" void * memmove(void *_Dst, const void *_Src, size_t _Size);
extern "C" void * memset(void *_Dst, int _Val, size_t _Size);
extern "C" int  memcmp(const void *_Buf1, const void *_Buf2, size_t _Size);


#define SYSTEM_MALLOC(size) malloc(size)
#define SYSTEM_FREE(ptr) free(ptr)
#define ME_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define ME_MEMMOVE(dst, src, size) memmove(dst, src, size)
#define ME_MEMCLEAR(dst, size) memset(dst, 0, size)
#define ME_MEMCMP(dst, src, size) memcmp(dst, src, size)