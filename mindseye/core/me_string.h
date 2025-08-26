#pragma once


struct StringView;
struct String
{
    char* data = nullptr;
    size_t len = 0;
    StringView CreateView(size_t offset = 0);
    StringView CreateView(size_t offset, size_t len);
    String(char* data, size_t len) : data(data), len(len) {};
    String(const char* data, size_t len) : data((char*)data), len(len) {};
    String(char* cstr);
	String(const char* cstr);
	String() = default;
    bool operator == (const String& sv) const;
    operator char*() { return data; }
	operator bool() const { return data && len; }
};

struct StringView // non-owning
{
    char* data = nullptr;
    size_t len = 0;
    StringView(const String&& s) { data = (char*)s.data; len = s.len; }
    StringView(const String& s) { data = (char*)s.data; len = s.len; }
    StringView() = default;
    StringView(char* data, size_t len) { this->data = data; this->len = len; };
    StringView(const char* data, size_t len) { this->data = (char*)data; this->len = len; };
	bool operator == (const StringView& sv) const;
    operator char*() { return data; }
	operator bool() const { return data && len; }

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

template <u64 N> 
String STRING_LIT(const char (&strlit)[N]) { return String{(char*)strlit, N-1}; }

#define STRING_VAARGS(str) str.len, str.data

MEAPI bool StringCopy(StringView dst, StringView src);

// returns -1 when needle isn't in haystack.
MEAPI s32 FindInString(
	StringView haystack,
	StringView needle,
	u32 offset = 0,
	StringCompareFlags flags = StringCompareFlags(0));

MEAPI s32 FindInStringRev(
	StringView haystack,
	StringView needle,
	u32 offset = 0,
	StringCompareFlags flags = StringCompareFlags(0));

MEAPI StringView EatChars(StringView str, char c);
MEAPI u32 EatCharsOffset(StringView str, char c);

// flags = bitfield of StringCompareFlags
MEAPI bool StringCompare(StringView str1, StringView str2, StringCompareFlags flags = StringCompareFlags(0));

MEAPI size_t CStringLength(const char* str);

MEAPI String StringFromCString(const char* str, s32 strLen = -1);

MEAPI size_t wcharToNarrow(const wchar_t* src, char * dest, size_t destLen);

MEAPI char ToLower(char c);

MEAPI const char* TextFormat(const char *text, ...);