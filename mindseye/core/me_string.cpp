
#include "core/me_defines.h"
#include "core/me_string.h"
#include "core/me_memory.h"
#include "core/me_log.h"

bool StringView::operator == (const StringView& sv) const 
{
    return sv.len == this->len && ME_MEMCMP(this->data, sv.data, sv.len) == 0;
}

StringView String::CreateView(size_t offset, size_t len) 
{ 
    return {data + offset, this->len < len ? this->len : len};
}

StringView String::CreateView(size_t offset) 
{ 
    offset = offset > len ? len : offset;
    return {data + offset, len - offset};
}

size_t CStringLength(const char* str)
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

bool StringCopy(String dst, String src)
{
    if (src.len > dst.len)
    {
        LOG_ERROR("Source string smaller than dst string!");
        return false;
    } 
    // source length will always be less than or equal to dst len
    size_t amountToCopy = src.len;
    ME_MEMCPY(dst.data, src.data, amountToCopy);
    return true;
}

StringView FindInString(StringView haystack, StringView needle)
{
    if (!needle.data || !needle.len || *needle.data == '\0') 
    {
        return haystack;
    }

    while (haystack.len && *haystack.data != '\0') 
    {
        if (needle == haystack) 
        {
            return haystack;
        }
        haystack = haystack.CreateView(1);
    }
    return {};
}

bool StringCompare(StringView str1, StringView str2, StringCompareFlags flags)
{
    if (str1.len != str2.len) return false;
    for (u64 i = 0; i < str1.len; i++)
    {
        char s1 = str1.data[i];
        char s2 = str2.data[i];
        if (flags & StringCompareFlags::CaseInsensitive)
        {
            s1 = ToLower(s1);
            s2 = ToLower(s2);
        }
        if (s1 != s2)
        {
            return false;
        }
    }
    return true;
}

char ToLower(char c)
{
    if (c >= 'A' && c <= 'Z') 
    {
        return c + 32;
    } else 
    {
        return c;
    }
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