
#include "core/me_defines.h"
#include "core/me_string.h"
#include "core/me_memory.h"
#include "core/me_log.h"
#include "external/stb/stb_sprintf.h"

// some compilers can't do offsetof in static asserts :/
//#include <cstddef> // For offsetof
// didn't want to factor these out into a separate struct
//STATIC_ASSERT(offsetof(String, data) == offsetof(StringView, data));
//STATIC_ASSERT(sizeof(String::data) == sizeof(StringView::data));
//STATIC_ASSERT(offsetof(String, len) == offsetof(StringView, len));
//STATIC_ASSERT(sizeof(String::len) == sizeof(StringView::len));

//STATIC_ASSERT(offsetof(meSpan, data) == offsetof(StringView, data));
//STATIC_ASSERT(sizeof(meSpan::data) == sizeof(StringView::data));
//STATIC_ASSERT(offsetof(meSpan, size) == offsetof(StringView, len));
//STATIC_ASSERT(sizeof(meSpan::size) == sizeof(StringView::len));

//STATIC_ASSERT(offsetof(meSpan, data) == 0);
//STATIC_ASSERT(offsetof(meSpan, size) == sizeof(meSpan::data));


MEMAP_BEGIN_CUSTOM_HASHER(StringView, obj) 
{
    size_t h1 = HashBytesL((u8*)obj.data, obj.len);
    size_t h2 = HashBytes((u8*)&obj.len, sizeof(obj.len));
    return h1 ^ (h2 << 1);
}
MEMAP_END_CUSTOM_HASHER

MEMAP_BEGIN_CUSTOM_HASHER(String, obj) 
{
    size_t h1 = HashBytesL((u8*)obj.data, obj.len);
    size_t h2 = HashBytes((u8*)&obj.len, sizeof(obj.len));
    return h1 ^ (h2 << 1);
}
MEMAP_END_CUSTOM_HASHER

bool StringView::operator==(const StringView& sv) const 
{
    return sv.len == this->len && ME_MEMCMP(this->data, sv.data, sv.len) == 0;
}

bool StringView::operator != (const StringView& sv) const
{
    return !operator==(sv);    
}

bool String::operator==(const String& s) const
{
	return operator==((StringView)s);
}

bool String::operator==(const StringView& s) const
{
    return s.len == this->len && ME_MEMCMP(this->data, s.data, s.len) == 0;
}

String::~String()
{
	if (allocator && data)
	{
		MEFREE(allocator, data);
	}
	data = nullptr;
	len = 0;
	allocator = nullptr;
}

String::String(const String& other)
{
	if (!allocator)
	{
		allocator = other.allocator;
	}
	CopyOf(other);
}

String& String::operator=(const String& other)
{
	if (this == &other) return *this; // self-assignment check

	// Free existing data
	if (allocator && data)
	{
		MEFREE(allocator, data);
	}

	if (!allocator)
	{
		allocator = other.allocator;
	}
	CopyOf(other);
	return *this;
}


String::String(String&& other) noexcept
{
	data = other.data;
	len = other.len;
	allocator = other.allocator;
	other.data = nullptr;
	other.len = 0;
	other.allocator = nullptr;
}

String& String::operator=(String&& other)
{
	if (this == &other) return *this; // self-assignment check

	// Free existing data
	if (allocator && data)
	{
		MEFREE(allocator, data);
	}

	data = other.data;
	len = other.len;
	allocator = other.allocator;
	other.data = nullptr;
	other.len = 0;
	other.allocator = nullptr;
	return *this;
}

static meAllocator* GetStringAllocator()
{
	// TODO
	return GetDefaultAllocator();
}

String& String::operator=(const StringView& other)
{
	if (!allocator)
	{
		allocator = GetStringAllocator();
	}

	// Allocate and copy new data before freeing — other.data may alias our buffer
	char* oldData = data;
	CopyOf(other, allocator);

	if (oldData && oldData != data)
	{
		MEFREE(allocator, oldData);
	}
	return *this;
}

String& String::operator=(const StringBuilder& other)
{
	if (!allocator)
	{
		allocator = other.allocator ? other.allocator : GetStringAllocator();
	}

	// Allocate and copy new data before freeing — other.data may alias our buffer
	char* oldData = data;
	CopyOf(StringView(other.data, other.len), allocator);

	if (oldData && oldData != data)
	{
		MEFREE(allocator, oldData);
	}
	return *this;
}


static void InitFromBuf(String* str, const char* data, size_t len, meAllocator* allocator)
{
    if (!allocator)
    {
        allocator = GetStringAllocator();
    }
	if (len == 0)
	{
		*str = String();
		return;
	}
	str->data = MEALLOC(allocator, len + 1);
	ME_MEMCPY(str->data, data, len);
	str->data[len] = '\0';
	str->len = len;
	str->allocator = allocator;
}

String::String(const char* data, size_t len, meAllocator* allocator)
{
	InitFromBuf(this, data, len, allocator);
}

String::String(const StringBuilder& builder, meAllocator* allocator)
{
	InitFromBuf(this, builder.data, builder.len, allocator ? allocator : builder.allocator);
}

String::String(const StringView& str, meAllocator* allocator)
{
	if (!allocator)
	{
		allocator = GetStringAllocator();
	}
	InitFromBuf(this, str.data, str.len, allocator);
}

void String::CopyOfCStr(const char* cstr, meAllocator* allocator)
{
	len = CStringLength(cstr);
	data = (char*)MEALLOC(allocator, len+1);
	data[len] = '\0';
	this->allocator = allocator;
	StringCopy(*this, StringView(cstr, len));
}

void String::CopyOf(const String& str)
{
	if (!str) { data = nullptr; len = 0; allocator = nullptr; return; }
	this->len = str.len;
	this->allocator = str.allocator;
	this->data = (char*)MEALLOC(str.allocator, len+1);
	this->data[this->len] = '\0';
	StringCopy(*this, str);
}

void String::CopyOf(const StringView& str, meAllocator* allocator)
{
	if (!str) { data = nullptr; len = 0; this->allocator = nullptr; return; }
	len = str.len;
	this->allocator = allocator;
	data = (char*)MEALLOC(this->allocator, len+1);
	this->data[this->len] = '\0';
	StringCopy(*this, str);
}

StringView String::OffsetView(size_t offset, size_t len) 
{ 
    return {(char*)data + offset, this->len < len ? this->len : len};
}

StringView String::OffsetView(size_t offset) 
{ 
    offset = offset > len ? len : offset;
    return {(char*)data + offset, len - offset};
}

const char* StringView::cstrForce(meAllocator* allocator) const
{
	if (data[len] == '\0') return data;
	Allocation a = MEALLOC(allocator, len + 1);
	a.data[len] = '\0';
	BufferCopy(a, meSpan(data, len));
	return a.data;
}

size_t CStringLength(const char* str)
{
	if (!str) return 0;
    size_t len = 0;
    while (str[len] != '\0') 
    {
        len++;
    }
    return len;
}

StringView StringFromCString(const char* str, u32 strLen)
{
	strLen = MEMIN(strLen, CStringLength(str));
    StringView result = {(char*)str, static_cast<size_t>(strLen)};
    return result;
}

const char* CStringFromString(
	StringView str, 
	meAllocator* allocator)
{
	const char* cstr = MEALLOC(allocator, str.len + 1);
	ME_MEMCPY((void*)cstr, str.data, str.len);
	str.data[str.len] = '\0';
	return cstr;
}

bool StringCopy(StringView dst, StringView src)
{
	bool result = BufferCopy(dst.ToSpan(), src.ToSpan());
	dst.data[src.len] = '\0';
	return result;
}

s32 FindInString(
	StringView haystack, 
	StringView needle, 
	u32 offset,
	StringOpFlags flags)
{
    if (!needle.data || !needle.len || *needle.data == '\0') 
    {
		return -1;
    }
	for (s32 hayStackIdx = (s32)offset; hayStackIdx <= (s64)haystack.len - (s64)needle.len; hayStackIdx++)
	{
		const char* haystackPtr = &haystack.data[hayStackIdx];
		u32 i = 0;
		for (; i < needle.len; i++)
		{
			char s1 = needle.data[i];
			char s2 = haystackPtr[i];
			if (flags & StringOpFlags_CaseInsensitive)
			{
				s1 = ToLower(s1);
				s2 = ToLower(s2);
			}
			if (s1 != s2)
			{
				break;
			}
		}
		// all matched
		if (i == needle.len)
		{
			return flags & StringOpFlags_IdxAfterNeedle ? hayStackIdx + needle.len : hayStackIdx;
		}
	}
	return -1;
}

s32 FindInStringRev(
	StringView haystack,
	StringView needle,
	u32 offsetFromBack,
	StringOpFlags flags)
{
	if (!needle.data || !needle.len || *needle.data == '\0') 
    {
		return -1;
    }
    s32 hayStackIdx = 0;
	for (hayStackIdx = ((s64)haystack.len) - needle.len - (s32)offsetFromBack; hayStackIdx >= 0; hayStackIdx--)
	{
		const char* haystackPtr = &haystack.data[hayStackIdx];
		u32 i = 0;
		for (; i < needle.len; i++)
		{
			char s1 = needle.data[i];
			char s2 = haystackPtr[i];
			if (flags & StringOpFlags_CaseInsensitive)
			{
				s1 = ToLower(s1);
				s2 = ToLower(s2);
			}
			if (s1 != s2)
			{
				break;
			}
		}
		// all matched
		if (i == needle.len)
		{
			return flags & StringOpFlags_IdxAfterNeedle ? hayStackIdx + 1 : hayStackIdx;
		}
	}
	return -1;
}

// TODO: "invert" is a confusing name here, maybe something like skipCharsInSet?
StringView EatChars(StringView str, StringView chars, bool invert)
{
    u32 offset = EatCharsOffset(str, chars, invert);
	StringView result = str.OffsetView(offset);
	return result;
}

u32 EatCharsOffset(StringView str, StringView chars, bool invert)
{
	u32 result = 0;
	while (str)
	{
        StringView thisChar = StringView(&str.data[0], 1);
        bool matched = (invert ? FindInString(chars, thisChar) == -1 : FindInString(chars, thisChar) != -1);
        if (!matched)
        {
            break;
        }
		str = str.OffsetView(1);
		result++;
	}
	return result;
}

StringView StringTrim(StringView str, StringView chars)
{
	for (u64 i = 0; i < chars.len; i++)
	{
		str = EatChars(str, chars[i]);
	}
	s64 i = str.len;
	for (; i > 0; i--)
	{
		char c = str[i - 1];
		for (u64 j = 0; j < chars.len; j++)
		{
			if (c != chars[j])
			{
				goto end;
			}
		}
	}
	end:
	if (str.len > (u64)i)
	{
		str[i] = '\0';
	}
	str.len = i;
	return str;
}

bool StringCompare(
	StringView str1, 
	StringView str2, 
	StringOpFlags flags)
{
    if (str1.len != str2.len) return false;
    for (u64 i = 0; i < str1.len; i++)
    {
        char s1 = str1.data[i];
        char s2 = str2.data[i];
        if (flags & StringOpFlags_CaseInsensitive)
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

void ToLower(StringView str)
{
	for (u64 i = 0; i < str.len; i++)
	{
		str[i] = ToLower(str[i]);
	}
}

char ToUpper(char c)
{
	if (c >= 'a' && c <= 'z') 
	{
        return c - 32;
    } 
	else 
	{
        return c;
    }
}

void ToUpper(StringView str)
{
	for (u64 i = 0; i < str.len; i++)
	{
		str[i] = ToUpper(str[i]);
	}
}

void StringReplace(StringView str, char oldC, char newC)
{
	for (u64 i = 0; i < str.len; i++)
	{
		if (str[i] == oldC)
		{
			str[i] = newC;
		}
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

static unsigned int stringToUint(const char *str, u32 len) {
    unsigned int result = 0;
    u32 i = 0;

    // Handle leading whitespace (optional, but good practice)
    while (isspace((unsigned char)str[i]) && i < len) {
        i++;
    }

    // Iterate through the string until a non-digit character or null terminator is found
    while (str[i] != '\0' && i < len && isdigit((unsigned char)str[i])) {
        unsigned int digit = str[i] - '0';

        // Check for potential overflow before multiplication
        if (result > UINT_MAX / 10 || (result == UINT_MAX / 10 && digit > UINT_MAX % 10)) {
            fprintf(stderr, "Warning: Integer overflow during conversion.\n");
            return UINT_MAX; // Return max value on overflow, or handle error as appropriate
        }

        result = result * 10 + digit;
        i++;
    }

    return result;
}

MEAPI u32 StringToUint(StringView str)
{
	// TOD:O
	return stringToUint(str.data, str.len);
}

StringView ScanForBalancedChar(StringView str, char opening, char closing)
{
	// opening= ( closing= )   : (something="another(thing)")andmore -> something="another(thing)"
	// opening= " closing= "   : "something="another"and" -> something=
	ME_ASSERT(str[0] == opening);
	ME_ASSERT(str.len > 1);
	s32 balance = 1;
	u64 i = 1;
	for (;i < str.len; i++)
	{
		if (str[i] == closing)
		{
			balance--;
		}
		else if (str[i] == opening)
		{
			balance++;
		}
		if (balance == 0)
		{
			break;
		}
	}
	StringView result = str.OffsetView(1, i - 1);
	return result;
}

StringBuilder::StringBuilder(
	meAllocator* allocator, 
	u32 initialSize,
	IsScopedAlloc isScopedAlloc)
{
	this->allocator = allocator;
	this->data = MEALLOC(allocator, initialSize);
	this->len = 0;
	this->capacity = initialSize;
	this->isScopedAlloc = isScopedAlloc;
}

StringBuilder::~StringBuilder()
{
	if (isScopedAlloc)
	{
		MEFREE(allocator, data);
	}
	len = 0;
	data = 0;
}

void StringBuilderCheckGrow(StringBuilder& sb, const StringView& sv)
{
	if (sb.len + sv.len + 1 > sb.capacity)
	{
		char* olddata = sb.data;
		sb.capacity = MEMAX(sb.capacity + sv.len, sb.capacity * 2);
		sb.data = MEALLOC(sb.allocator, sb.capacity);
		ME_MEMCPY(sb.data, olddata, sb.len);
		sb.allocator->meFree(olddata);
	}
}

void StringBuilder::Append(StringView str)
{
    if (!str)
    {
        return;
    }
	ME_ASSERT(allocator);
	StringBuilderCheckGrow(*this, str);
	ME_MEMCPY(data + len, str.data, str.len);
	len += str.len;
}

#ifndef MAX_TEXTFORMAT_BUFFERS
#define MAX_TEXTFORMAT_BUFFERS      12        // Maximum number of static buffers for text formatting
#endif
#ifndef MAX_TEXT_BUFFER_LENGTH
#define MAX_TEXT_BUFFER_LENGTH   16000        // Maximum size of static text buffer
#endif

// yoinked from raylib
const char* InternalStringFormat(
	const char *text, 
	va_list* args, 
	s32& numBytesWritten,
	meAllocator* allocator = nullptr)
{
    // We create an array of buffers so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static thread_local char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { {0} };
    static thread_local int index = 0;

	char* currentBuffer = nullptr;
	// NOTE: stbsp_vsnprintf always null-terminates, and DOES NOT include the null terminator in numBytesWritten
	if (allocator)
	{
		char tempBuf[MAX_TEXT_BUFFER_LENGTH];
		numBytesWritten = stbsp_vsnprintf(tempBuf, MAX_TEXT_BUFFER_LENGTH, text, *args);
		currentBuffer = MEALLOC(allocator, numBytesWritten);
		ME_MEMCPY(currentBuffer, tempBuf, numBytesWritten);
	}
	else
	{
		currentBuffer = buffers[index++];
		numBytesWritten = stbsp_vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, *args);
	}
	ME_ASSERT((numBytesWritten + 1) < MAX_TEXT_BUFFER_LENGTH);

    if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;

    return currentBuffer;
}

s32 StringBuilder::AppendFormat(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
	s32 numBytesWritten = 0;
	const char* formattedTmpBuf = InternalStringFormat(fmt, &args, numBytesWritten, allocator);
    va_end(args);
	StringView stringToAppend = StringView(formattedTmpBuf, numBytesWritten);
	Append(stringToAppend);
	ME_ASSERT(capacity > len);
	return numBytesWritten;
}

void StringBuilder::Clear()
{
	data[0] = '\0';
	len = 0;
}

StringView StringFormatTmp(const char *text, ...)
{
    va_list args;
    va_start(args, text);
	s32 numBytesWritten = 0;
	const char* result = InternalStringFormat(text, &args, numBytesWritten);
    va_end(args);
	return { result, static_cast<u64>(numBytesWritten)};
}

s32 StringFormatIntoBuf(meSpan backingBuffer, const char *text, ...)
{
	ME_MEMCLEAR(backingBuffer.data, backingBuffer.size);

    va_list args;
    va_start(args, text);
    int bytes = stbsp_vsnprintf((char*)backingBuffer.data, backingBuffer.size, text, args);
    va_end(args);
	return bytes;
}

StringView StringFormatNew(
	meAllocator* allocator, 
	const char *text, ...)
{
	char backing[MAX_TEXT_BUFFER_LENGTH];

    va_list args;
    va_start(args, text);
    s32 len = stbsp_vsnprintf(backing, MAX_TEXT_BUFFER_LENGTH, text, args);
    ME_ASSERT(len > 0);
    va_end(args);
    backing[len] = '\0';
	const char* result = MEALLOC(allocator, len + 1);
	ME_MEMCPY((void*)result, backing, len + 1);
	return {result, (u64)len};
}






// parsing

bool IsWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}


s32 StringParseInt32(StringView& str)
{
    if (!str.data || str.len == 0) return 0;
    
    str = EatChars(str, STRING_LIT(" "));
    
    if (str.len == 0) return 0;
    
    // Check for sign
    bool negative = false;
    if (str.data[0] == '-')
    {
        negative = true;
        str = str.OffsetView(1);
    }
    else if (str.data[0] == '+')
    {
        str = str.OffsetView(1);
    }
    
    if (str.len == 0) return 0;
    
    s32 result = 0;
    
    u64 i = 0;
    for (;i < str.len && IsDigit(str.data[i]); i++)
    {
        result = result * 10 + (str.data[i] - '0');
    }
    str = str.OffsetView(i);
    return negative ? -result : result;
}

u32 StringParseUInt32(StringView& str)
{
    if (!str.data || str.len == 0) return 0;
    
    str = EatChars(str, STRING_LIT(" "));
    
    if (str.len == 0) return 0;
    
    // Skip optional '+' sign
    if (str.data[0] == '+')
    {
        str = str.OffsetView(1);
    }
    
    if (str.len == 0) return 0;
    
    u32 result = 0;
    
    u64 i = 0;
    for (; i < str.len && IsDigit(str.data[i]); i++)
    {
        result = result * 10 + (str.data[i] - '0');
    }
    str = str.OffsetView(i);
    
    return result;
}

s64 StringParseInt64(StringView& str)
{
    if (!str.data || str.len == 0) return 0;
    
    str = EatChars(str, STRING_LIT(" "));
    
    if (str.len == 0) return 0;
    
    // Check for sign
    bool negative = false;
    if (str.data[0] == '-')
    {
        negative = true;
        str = str.OffsetView(1);
    }
    else if (str.data[0] == '+')
    {
        str = str.OffsetView(1);
    }
    
    if (str.len == 0) return 0;
    
    s64 result = 0;
    
    u64 i = 0;
    for (; i < str.len && IsDigit(str.data[i]); i++)
    {
        result = result * 10 + (str.data[i] - '0');
    }
    str = str.OffsetView(i);
    
    return negative ? -result : result;
}

u64 StringParseUInt64(StringView& str)
{
    if (!str.data || str.len == 0) return 0;
    
    str = EatChars(str, STRING_LIT(" "));
    
    if (str.len == 0) return 0;
    
    // Skip optional '+' sign
    if (str.data[0] == '+')
    {
        str = str.OffsetView(1);
    }
    
    if (str.len == 0) return 0;
    
    u64 result = 0;
    
    u64 i = 0;
    for (; i < str.len && IsDigit(str.data[i]); i++)
    {
        result = result * 10 + (str.data[i] - '0');
    }
    str = str.OffsetView(i);
    
    return result;
}

float StringParseFloat(StringView& str)
{
    if (!str.data || str.len == 0) return 0.0f;
    
    str = EatChars(str, STRING_LIT(" "));
    
    if (str.len == 0) return 0.0f;
    
    // Check for sign
    bool negative = false;
    if (str.data[0] == '-')
    {
        negative = true;
        str = str.OffsetView(1);
    }
    else if (str.data[0] == '+')
    {
        str = str.OffsetView(1);
    }
    
    if (str.len == 0) return 0.0f;
    
    // Check for special values
    if (str.len >= 3)
    {
        if ((str.data[0] == 'i' || str.data[0] == 'I') &&
            (str.data[1] == 'n' || str.data[1] == 'N') &&
            (str.data[2] == 'f' || str.data[2] == 'F'))
        {
            return negative ? -INFINITY : INFINITY;
        }
        
        if ((str.data[0] == 'n' || str.data[0] == 'N') &&
            (str.data[1] == 'a' || str.data[1] == 'A') &&
            (str.data[2] == 'n' || str.data[2] == 'N'))
        {
            return NAN;
        }
    }
    
    // Parse the number using float precision throughout
    float result = 0.0f;
    u64 i = 0;
    
    // Parse integer part
    while (i < str.len && IsDigit(str.data[i]))
    {
        result = result * 10.0f + (float)(str.data[i] - '0');
        i++;
    }
    
    // Parse fractional part
    if (i < str.len && str.data[i] == '.')
    {
        i++;
        float divisor = 10.0f;
        
        while (i < str.len && IsDigit(str.data[i]))
        {
            result += (float)(str.data[i] - '0') / divisor;
            divisor *= 10.0f;
            i++;
        }
    }
    
    // Parse exponent
    if (i < str.len && (str.data[i] == 'e' || str.data[i] == 'E'))
    {
        i++;
        
        bool expNegative = false;
        if (i < str.len && str.data[i] == '-')
        {
            expNegative = true;
            i++;
        }
        else if (i < str.len && str.data[i] == '+')
        {
            i++;
        }
        
        s32 exponent = 0;
        while (i < str.len && IsDigit(str.data[i]))
        {
            exponent = exponent * 10 + (str.data[i] - '0');
            i++;
        }
        
        if (expNegative) exponent = -exponent;
        
        // Apply exponent using float arithmetic
        float multiplier = 1.0f;
        if (exponent > 0)
        {
            for (s32 j = 0; j < exponent; j++)
            {
                multiplier *= 10.0f;
            }
        }
        else if (exponent < 0)
        {
            for (s32 j = 0; j < -exponent; j++)
            {
                multiplier /= 10.0f;
            }
        }
        
        result *= multiplier;
    }
    str = str.OffsetView(i);
    
    return negative ? -result : result;
}


double StringParseDouble(StringView& str)
{
    if (!str.data || str.len == 0) return 0.0;
    
    str = EatChars(str, STRING_LIT(" "));
    
    if (str.len == 0) return 0.0;
    
    // Check for sign
    bool negative = false;
    if (str.data[0] == '-')
    {
        negative = true;
        str = str.OffsetView(1);
    }
    else if (str.data[0] == '+')
    {
        str = str.OffsetView(1);
    }
    
    if (str.len == 0) return 0.0;
    
    // Check for special values
    if (str.len >= 3)
    {
        if ((str.data[0] == 'i' || str.data[0] == 'I') &&
            (str.data[1] == 'n' || str.data[1] == 'N') &&
            (str.data[2] == 'f' || str.data[2] == 'F'))
        {
            return negative ? -INFINITY : INFINITY;
        }
        
        if ((str.data[0] == 'n' || str.data[0] == 'N') &&
            (str.data[1] == 'a' || str.data[1] == 'A') &&
            (str.data[2] == 'n' || str.data[2] == 'N'))
        {
            return NAN;
        }
    }
    
    // Parse the number
    double result = 0.0;
    u64 i = 0;
    
    // Parse integer part
    while (i < str.len && IsDigit(str.data[i]))
    {
        result = result * 10.0 + (str.data[i] - '0');
        i++;
    }
    
    // Parse fractional part
    if (i < str.len && str.data[i] == '.')
    {
        i++;
        double fraction = 0.0;
        double divisor = 10.0;
        
        while (i < str.len && IsDigit(str.data[i]))
        {
            fraction += (str.data[i] - '0') / divisor;
            divisor *= 10.0;
            i++;
        }
        
        result += fraction;
    }
    
    // Parse exponent
    if (i < str.len && (str.data[i] == 'e' || str.data[i] == 'E'))
    {
        i++;
        
        bool expNegative = false;
        if (i < str.len && str.data[i] == '-')
        {
            expNegative = true;
            i++;
        }
        else if (i < str.len && str.data[i] == '+')
        {
            i++;
        }
        
        s32 exponent = 0;
        while (i < str.len && IsDigit(str.data[i]))
        {
            exponent = exponent * 10 + (str.data[i] - '0');
            i++;
        }
        
        if (expNegative) exponent = -exponent;
        
        // Apply exponent
        double multiplier = 1.0;
        if (exponent > 0)
        {
            for (s32 j = 0; j < exponent; j++)
            {
                multiplier *= 10.0;
            }
        }
        else if (exponent < 0)
        {
            for (s32 j = 0; j < -exponent; j++)
            {
                multiplier /= 10.0;
            }
        }
        
        result *= multiplier;
    }
    str = str.OffsetView(i);

    return negative ? -result : result;
}


// end parsing