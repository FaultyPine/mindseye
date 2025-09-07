#pragma once


struct StringView;
struct String
{
    char* data = nullptr;
    size_t len = 0;
    StringView OffsetView(size_t offset = 0);
    StringView OffsetView(size_t offset, size_t len);
    String(char* data, size_t len) : data(data), len(len) {};
    String(const char* data, size_t len) : data((char*)data), len(len) {};
	String() = default;
	void CopyOfCStr(const char* cstr, meAllocator* allocator);
	void CopyOf(const char* str, size_t len, meAllocator* allocator);
	void CopyOf(const String& str, meAllocator* allocator);
	void CopyOf(const StringView& str, meAllocator* allocator);
	bool operator == (const String& sv) const;
    operator char*() { return data; }
	operator bool() const { return data && len; }
};

struct StringView // non-owning
{
    char* data = nullptr;
    u64 len = 0;
    StringView(const String&& s) { data = (char*)s.data; len = s.len; }
    StringView(const String& s) { data = (char*)s.data; len = s.len; }
    StringView() = default;
    StringView(char* data, size_t len) { this->data = data; this->len = len; };
    StringView(const char* data, size_t len) { this->data = (char*)data; this->len = len; };
	bool operator == (const StringView& sv) const;
    operator char*() { return data; }
	operator bool() const
	{ 
		return data && len; 
	}
	char operator[](size_t idx) 
	{
		ME_ASSERT(idx < len);
		return data[idx]; 
	}

    StringView OffsetView(size_t offset = 0) 
    { 
        offset = offset > len ? len : offset;
        return {data + offset, len - offset};
    }
    StringView OffsetView(size_t offset, size_t len) 
    { 
        return {data + offset, this->len < len ? this->len : len};
    }
};

using StringOpFlags = u32;

enum StringOpFlags_Enum
{
    StringOpFlags_CaseInsensitive = NTH_BIT(0),
	StringOpFlags_IdxAfterNeedle = NTH_BIT(1),
};

template <u64 N> 
String STRING_LIT(const char (&strlit)[N]) { return String{(char*)strlit, N-1}; }

// %.*s
#define STRING_VAARGS(str) str.len, str.data

MEAPI bool StringCopy(StringView dst, StringView src);

// returns -1 when needle isn't in haystack.
MEAPI s32 FindInString(
	StringView haystack,
	StringView needle,
	u32 offset = 0,
	StringOpFlags flags = StringOpFlags(0));

MEAPI s32 FindInStringRev(
	StringView haystack,
	StringView needle,
	u32 offset = 0,
	StringOpFlags flags = StringOpFlags(0));

// invert meaning this eats anything except the given char
MEAPI StringView EatChars(StringView str, char c, bool invert = false);
// invert meaning this eats anything except the given char
MEAPI u32 EatCharsOffset(StringView str, char c, bool invert = false);

// flags = bitfield of StringCompareFlags
MEAPI bool StringCompare(StringView str1, StringView str2, StringOpFlags flags = StringOpFlags(0));

MEAPI size_t CStringLength(const char* str);

MEAPI String StringFromCString(const char* str, s32 strLen = -1);

MEAPI const char* CStringFromString(StringView str, meAllocator* allocator);

MEAPI size_t wcharToNarrow(const wchar_t* src, char * dest, size_t destLen);

// I.E. start on (, scan until matching balanced ) appears
// str[0] must be == opening
MEAPI StringView ScanForBalancedChar(StringView str, char opening, char closing);

MEAPI char ToLower(char c);

MEAPI const char* TextFormat(const char *text, ...);