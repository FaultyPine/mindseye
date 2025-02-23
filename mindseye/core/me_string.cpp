
#include "core/me_defines.h"
#include "me_string.h"
#include <string>

static size_t CStringLength(const char* str)
{
    size_t len = 0;
    while (str[len] != '\0') 
    {
        len++;
    }
    return len;
}

String FromCString(const char* str, s32 strLen)
{
    if (strLen == -1)
    {
        strLen = CStringLength(str);
    }
    String result = {(char*)str, static_cast<size_t>(strLen)};
    return result;
}

Result<void, size_t> StringCopy(String dst, String src)
{
    if (src.len > dst.len)
    {
        return Err(src.len);
    } 
    for (int i = 0; i < dst.len && i < src.len; i++)
    {
        
    }
    return Ok();
}

size_t wcharToNarrow(const wchar_t * src, char * dest, size_t dest_len)
{
    size_t i;
    wchar_t code;
    i = 0;
    while (src[i] != '\0' && i < (dest_len - 1))
    {
        code = src[i];
        if (code < 128)
        {
            dest[i] = char(code);
        }
        else
        {
        dest[i] = '?';
        if (code >= 0xD800 && code <= 0xDBFF)
            // lead surrogate, skip the next code unit, which is the trail
            i++;
        }
        i++;
    }
    dest[i] = '\0';
    return i - 1;
}