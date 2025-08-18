#pragma once


struct StringView;
struct String
{
    char* data = nullptr;
    size_t len = 0;
    StringView CreateView(size_t offset = 0);
    StringView CreateView(size_t offset, size_t len);
    String(char* data, size_t len) : data(data), len(len) {};
    String() = default;
    operator char*() { return data; }
};

struct StringView // non-owning
{
    char* data = nullptr;
    size_t len = 0;
    StringView(const String&& s) { data = (char*)s.data; len = s.len; }
    StringView(const String& s) { data = (char*)s.data; len = s.len; }
    StringView() = default;
    StringView(char* data, size_t len) { this->data = data; this->len = len; };
    bool operator == (const StringView& sv) const;

    StringView CreateView(size_t offset = 0) 
    { 
        offset = offset > len ? len : offset;
        return {data + offset, len - offset};
    }
    StringView CreateView(size_t offset, size_t len) 
    { 
        return {data + offset, this->len < len ? this->len : len};
    }
};

enum StringCompareFlags
{
    CaseInsensitive = NTH_BIT(0)
};

#define STRING_LIT(strlit) (String{(char*)(strlit), sizeof(strlit)-1})


bool StringCopy(String dst, String src);

StringView FindInString(StringView haystack, StringView needle);

// flags = bitfield of StringCompareFlags
bool StringCompare(StringView str1, StringView str2, StringCompareFlags flags = StringCompareFlags(0));

size_t CStringLength(const char* str);

String FromCString(const char* str, s32 strLen = -1);

size_t wcharToNarrow(const wchar_t* src, char * dest, size_t destLen);

char ToLower(char c);
