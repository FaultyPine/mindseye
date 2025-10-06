#pragma once

struct meAllocator;
struct StringView;
struct String
{
    char* data = nullptr;
    size_t len = 0;
	meAllocator* allocator = nullptr;

    MEAPI StringView OffsetView(size_t offset = 0);
    MEAPI StringView OffsetView(size_t offset, size_t len);
    MEAPI String(const char* data, size_t len, meAllocator* allocator);
    MEAPI String(size_t len, meAllocator* allocator);
    MEAPI String(const StringView& str, meAllocator* allocator = nullptr);
	MEAPI String() = default;

	MEAPI ~String();
	MEAPI String(const String& other);// copy
	MEAPI String& operator=(const String& other); // copy assignment
	MEAPI String(String&& other) noexcept; // move
	MEAPI String& operator=(String&& other); // move assignment

	MEAPI void CopyOfCStr(const char* cstr, meAllocator* allocator);
	MEAPI void CopyOf(const String& str);
	MEAPI void CopyOf(const StringView& str, meAllocator* allocator);
	
	MEAPI bool operator == (const String& sv) const;
	MEAPI bool operator == (const StringView& sv) const;
    MEAPI explicit operator char*() { return data; }
	MEAPI explicit operator bool() const { return data && len; }

	const char* cstr() const 
	{
		ME_ASSERT(data[len] == '\0');
		return data;
	}
};


struct StringBuilder
{
	char* data = nullptr;
	u64 len = 0;
	u64 capacity = 0;
	meAllocator* allocator = nullptr;

	StringBuilder(meAllocator* allocator, u32 initialSize = 1024);
	~StringBuilder();

	void SetAllocator(meAllocator* allocator) { this->allocator = allocator; }
	void Append(StringView str);
	void AppendFormat(const char* fmt, ...);
};

struct StringView
{
    char* data = nullptr;
    u64 len = 0;
    MEAPI StringView() = default;
    MEAPI StringView(const String&& s) { data = (char*)s.data; len = s.len; }
    MEAPI StringView(const String& s) { data = (char*)s.data; len = s.len; }
    MEAPI StringView(const StringBuilder& s) { data = (char*)s.data; len = s.len; }
	MEAPI StringView(const char* data, size_t len) { this->data = (char*)data; this->len = len; };
	MEAPI bool operator == (const StringView& sv) const;
    explicit operator char*() { return data; }
	explicit operator bool() const
	{ 
		return data && len; 
	}
	char& operator[](size_t idx) 
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

	const char* cstr() const 
	{
		ME_ASSERT(data[len] == '\0');
		return data;
	}
};

using StringOpFlags = u32;

enum StringOpFlags_Enum
{
    StringOpFlags_CaseInsensitive = NTH_BIT(0),
	StringOpFlags_IdxAfterNeedle = NTH_BIT(1),
};

template <u64 N> 
StringView STRING_LIT(const char (&strlit)[N]) { return StringView{(char*)strlit, N-1}; }

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

MEAPI StringView StringFromCString(const char* str, s32 strLen = -1);

MEAPI const char* CStringFromString(StringView str, meAllocator* allocator);

MEAPI size_t wcharToNarrow(const wchar_t* src, char * dest, size_t destLen);

// I.E. start on (, scan until matching balanced ) appears
// str[0] must be == opening
MEAPI StringView ScanForBalancedChar(StringView str, char opening, char closing);

MEAPI char ToLower(char c);
MEAPI void ToLower(StringView str);

MEAPI char ToUpper(char c);
MEAPI void ToUpper(StringView str);

MEAPI void StringReplace(StringView str, char oldC, char newC);

MEAPI u32 StringToUint(StringView str);

// formats a string. Returned string buffer is a temporary buffer
// that will be evicted on the next couple calls to this function
// note, the returned string will be null-terminated
MEAPI StringView StringFormat(const char *text, ...);
// formats a string into an existing buffer, returns size of the string written to the buffer
MEAPI s32 StringFormatIntoBuf(meSpan backingBuffer, const char *text, ...);
// same as above, but allocates memory for the formatted string
MEAPI StringView StringFormatNew(meAllocator* allocator, const char *text, ...);



// (decimal only)
MEAPI s32 StringParseInt32(StringView str);
// (decimal only)
MEAPI u32 StringParseUInt32(StringView str);
// (decimal only)
MEAPI s64 StringParseInt64(StringView str);
// (decimal only)
MEAPI u64 StringParseUInt64(StringView str);
// (supports scientific notation)
MEAPI float StringParseFloat(StringView str);
// (supports scientific notation)
MEAPI double StringParseDouble(StringView str);
