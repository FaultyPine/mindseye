#pragma once

#include "core/me_result.h"

struct String
{
    char* data;
    size_t len;
};


#define STRING_LIT(strlit) (String{(char*)(strlit), sizeof(strlit)-1})

String FromCString(const char* str, s32 strLen = -1);
Result<void, size_t> StringCopy(String dst, String src);


size_t wcharToNarrow(const wchar_t * src, char * dest, size_t destLen);