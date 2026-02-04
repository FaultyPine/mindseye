#pragma once

#include "mindseye/core/me_core.h"
#include "mindseye/core/containers/me_span.h"

// TODO: String allocator

struct meAllocator;
struct StringView;
struct StringBuilder;
struct String
{
    char* data = nullptr;
    u64 len = 0;
	meAllocator* allocator = nullptr;

    MEAPI StringView OffsetView(u64 offset = 0);
    MEAPI StringView OffsetView(u64 offset, u64 len);
    MEAPI String(const char* data, u64 len, meAllocator* allocator);
    MEAPI String(u64 len, meAllocator* allocator);
    MEAPI String(const StringView& str, meAllocator* allocator = nullptr);
	MEAPI String(const StringBuilder& builder, meAllocator* allocator = nullptr);
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

// despite allocating through the passed-in allocator in the ctor, we don't free it in dtor
// I.E. the StringBuilder is not responsible for the memory lifetime of its data
// it is used to create & build the string, then pass it off to someone else
struct StringBuilder
{
	char* data = nullptr;
	u64 len = 0;
	u64 capacity = 0;
	meAllocator* allocator = nullptr;
	bool isScopedAlloc = false;

	enum IsScopedAlloc : bool;
	MEAPI StringBuilder() = default;
	MEAPI StringBuilder(
		meAllocator* allocator, 
		u32 initialSize = 1024,
		IsScopedAlloc isScopedAlloc = IsScopedAlloc(false));
	MEAPI ~StringBuilder();

	MEAPI void SetAllocator(meAllocator* allocator) { this->allocator = allocator; }
	MEAPI void Append(StringView str);
	MEAPI s32 AppendFormat(const char* fmt, ...);
	MEAPI void Clear();
};

struct StringView
{
    char* data = nullptr;
    u64 len = 0;
    MEAPI StringView() = default;
    MEAPI StringView(const String&& s) { data = (char*)s.data; len = s.len; }
    MEAPI StringView(const String& s) { data = (char*)s.data; len = s.len; }
    MEAPI StringView(const StringBuilder& s) { data = (char*)s.data; len = s.len; }
	MEAPI constexpr StringView(const char* data, u64 len) { this->data = (char*)data; this->len = len; };
	MEAPI explicit StringView(const meSpan& span) { this->data = (char*)span.data; this->len = span.size; }
	MEAPI bool operator == (const StringView& sv) const;
	MEAPI bool operator != (const StringView& sv) const;
	explicit operator bool() const
	{ 
		return data != nullptr && len > 0;
	}
	char& operator[](u64 idx) 
	{
		ME_ASSERT(idx < len);
		return data[idx]; 
	}

	// for strings, subspans that exceed the string length get silently clamped
    StringView OffsetView(u64 offset = 0) 
    {
        offset = offset > len ? len : offset;
        return {data + offset, len - offset};
    }
	// for strings, subspans that exceed the string length get silently clamped
    StringView OffsetView(u64 offset, u64 len) 
    { 
        return {data + offset, this->len < len ? this->len : len};
    }
	static StringView FromSpan(const meSpan& span)
	{
		return StringView(span.data, span.size);
	}
	meSpan ToSpan() const
	{
		return meSpan(data, len);
	}

	const char* cstr() const 
	{
		ME_ASSERT(data[len] == '\0');
		return data;
	}
	const char* cstrForce(meAllocator* allocator) const;
};

using StringOpFlags = u32;

enum StringOpFlags_Enum
{
    StringOpFlags_CaseInsensitive = NTH_BIT(0),
	StringOpFlags_IdxAfterNeedle = NTH_BIT(1),
};

template <u64 N> 
constexpr StringView STRING_LIT(const char (&strlit)[N]) { return StringView{(char*)strlit, N-1}; }

#define STRING_VAARGS(str) (s32)str.len, str.data
#define STRING_FMT "%.*s"

bool IsWhitespace(char c);
bool IsDigit(char c);

MEAPI bool StringCopy(
	StringView dst, 
	StringView src);

// returns -1 when needle isn't in haystack.
MEAPI s32 FindInString(
	StringView haystack,
	StringView needle,
	u32 offset = 0,
	StringOpFlags flags = StringOpFlags(0));

constexpr s32 ConstexprStrstr(StringView haystack, StringView needle)
{
	if (!needle.data || !needle.len || *needle.data == '\0') 
	{
		return -1;
	}
	u64 maxLen = haystack.len - needle.len;
	for (u64 haystackIdx = 0; haystackIdx <= maxLen; haystackIdx++)
	{
		if (haystack.data[haystackIdx] == *needle.data) 
		{
			u64 h_idx = haystackIdx;
			u64 n_idx = 0;
			
			while (h_idx < haystack.len && n_idx < needle.len && haystack.data[h_idx] == needle.data[n_idx]) 
			{
				h_idx++;
				n_idx++;
			}
			// If we reached end of needle, it's a match
			if (n_idx == needle.len)
			{
				return (s32)haystackIdx;
			}
		}
	}
	return -1;
}

MEAPI s32 FindInStringRev(
	StringView haystack,
	StringView needle,
	u32 offsetFromBack = 0,
	StringOpFlags flags = StringOpFlags(0));

// invert meaning this eats anything except the given char
MEAPI StringView EatChars(
	StringView str, 
	StringView chars, 
	bool invert = false);
StringView inline EatChars(
	StringView str, 
	char c, 
	bool invert = false)
{
	return EatChars(str, StringView(&c, 1), invert);
}
// invert meaning this eats anything except the given char
MEAPI u32 EatCharsOffset(
	StringView str, 
	StringView chars, 
	bool invert = false);
MEAPI inline u32 EatCharsOffset(
	StringView str, 
	char c, 
	bool invert = false)
{
	return EatCharsOffset(str, StringView(&c, 1), invert);
}

MEAPI StringView StringTrim(
	StringView str, 
	StringView chars);

// flags = bitfield of StringCompareFlags
MEAPI bool StringCompare(
	StringView str1, 
	StringView str2, 
	StringOpFlags flags = StringOpFlags(0));

MEAPI u64 CStringLength(
	const char* str);

MEAPI StringView StringFromCString(
	const char* str, 
	u32 strLen = (u32)-1);

MEAPI const char* CStringFromString(
	StringView str, 
	meAllocator* allocator);

MEAPI u64 wcharToNarrow(
	const wchar_t* src, 
	char * dest, 
	u64 destLen);

// I.E. start on (, scan until matching balanced ) appears
// str[0] must be == opening
MEAPI StringView ScanForBalancedChar(
	StringView str, 
	char opening, 
	char closing);

MEAPI char ToLower(char c);
MEAPI void ToLower(StringView str);

MEAPI char ToUpper(char c);
MEAPI void ToUpper(StringView str);

MEAPI void StringReplace(StringView str, char oldC, char newC);

MEAPI u32 StringToUint(StringView str);

// formats a string. Returned string buffer is a temporary buffer
// that will be evicted on the next couple calls to this function
// note, the returned string will be null-terminated
MEAPI StringView StringFormatTmp(const char *text, ...);
// formats a string into an existing buffer, returns size of the string written to the buffer
MEAPI s32 StringFormatIntoBuf(meSpan backingBuffer, const char *text, ...);
// same as above, but allocates memory for the formatted string
MEAPI StringView StringFormatNew(meAllocator* allocator, const char *text, ...);


// TODO: parsing functions modify the input StringView to consume the parsed portion

// (decimal only)
MEAPI s32 StringParseInt32(StringView& str);
// (decimal only)
MEAPI u32 StringParseUInt32(StringView& str);
// (decimal only)
MEAPI s64 StringParseInt64(StringView& str);
// (decimal only)
MEAPI u64 StringParseUInt64(StringView& str);
// (supports scientific notation)
MEAPI float StringParseFloat(StringView& str);
// (supports scientific notation)
MEAPI double StringParseDouble(StringView& str);
