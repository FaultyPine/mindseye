
#include "core/me_defines.h"
#include "core/me_string.h"
#include "core/me_memory.h"
#include "core/me_log.h"
#include "external/stb/stb_sprintf.h"

bool StringView::operator==(const StringView& sv) const 
{
    return sv.len == this->len && ME_MEMCMP(this->data, sv.data, sv.len) == 0;
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
}

String& String::operator=(String&& other)
{
	data = other.data;
	len = other.len;
	allocator = other.allocator;
	return *this;
}


static void InitFromBuf(String* str, const char* data, size_t len, meAllocator* allocator)
{
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

String::String(size_t len, meAllocator* allocator)
{
	this->data = MEALLOC(allocator, len + 1);
	this->len = len;
	this->allocator = allocator;
}


String::String(const StringView& str, meAllocator* allocator)
{
	if (!allocator)
	{
		allocator = GetSystemAllocator();
	}
	InitFromBuf(this, str.data, str.len, allocator);
}

void String::CopyOfCStr(const char* cstr, meAllocator* allocator)
{
	len = CStringLength(cstr);
	data = (char*)MEALLOC(allocator, len);
	this->allocator = allocator;
	StringCopy(*this, StringView(cstr, len));
}

void String::CopyOf(const String& str)
{
	if (!str) { *this = {}; return; }
	this->len = str.len;
	data = (char*)MEALLOC(str.allocator, len);
	allocator = str.allocator;
	StringCopy(*this, str);
}

void String::CopyOf(const StringView& str, meAllocator* allocator)
{
	if (!str) { *this = {}; return; }
	this->len = str.len;
	data = (char*)MEALLOC(allocator, len);
	this->allocator = allocator;
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

size_t CStringLength(const char* str)
{
    size_t len = 0;
    while (str[len] != '\0') 
    {
        len++;
    }
    return len;
}

StringView StringFromCString(const char* str, s32 strLen)
{
    if (strLen == -1)
    {
        strLen = CStringLength(str);
    }
    StringView result = {(char*)str, static_cast<size_t>(strLen)};
    return result;
}

const char* CStringFromString(
	StringView str, 
	meAllocator* allocator)
{
	const char* cstr = MEALLOC(allocator, str.len + 1);
	ME_MEMCLEAR((void*)cstr, str.len + 1);
	ME_MEMCPY((void*)cstr, str.data, str.len);
	return cstr;
}

bool StringCopy(StringView dst, StringView src)
{
    if (src.len > dst.len)
    {
        LOG_ERROR("Source string smaller than dst string!");
        return false;
    } 
    // source length will always be less than or equal to dst len
    size_t amountToCopy = src.len;
    ME_MEMCPY((void*)dst.data, src.data, amountToCopy);
    return true;
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
	u32 offset,
	StringOpFlags flags)
{
	if (!needle.data || !needle.len || *needle.data == '\0') 
    {
		return -1;
    }
	for (s32 hayStackIdx = ((s64)haystack.len) - 1 - (s32)offset; hayStackIdx - needle.len >= 0; hayStackIdx--)
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


StringView EatChars(StringView str, char c, bool invert)
{
	u32 offset = EatCharsOffset(str, c, invert);
	StringView result = str.OffsetView(offset);
	return result;
}

u32 EatCharsOffset(StringView str, char c, bool invert)
{
	u32 result = 0;
	while (invert ? str.data[0] != c : str.data[0] == c)
	{
		str = str.OffsetView(1);
		result++;
	}
	return result;
}

bool StringCompare(StringView str1, StringView str2, StringOpFlags flags)
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

StringBuilder::StringBuilder(meAllocator* allocator, u32 initialSize)
{
	this->allocator = allocator;
	this->data = MEALLOC(allocator, initialSize);
	this->len = 0;
	this->capacity = initialSize;
}

StringBuilder::~StringBuilder()
{
	MEFREE(allocator, data);
	len = 0;
	data = 0;
}

void StringBuilderCheckGrow(StringBuilder& sb, const StringView& sv)
{
	if (sb.len + sv.len > sb.capacity)
	{
		char* olddata = sb.data;
		sb.capacity = MEMAX(sb.capacity + sv.len, sb.capacity * 2);
		sb.data = MEALLOC(sb.allocator, sb.capacity);
		ME_MEMCPY(sb.data, olddata, sb.len);
	}
}

void StringBuilder::Append(StringView str)
{
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
const char* InternalStringFormat(const char *text, va_list* args, s32& numBytesWritten)
{
    // We create an array of buffers so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { {0} };
    static int index = 0;

    char *currentBuffer = buffers[index];

    numBytesWritten = stbsp_vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, *args);
	ME_ASSERT((numBytesWritten + 1) < MAX_TEXT_BUFFER_LENGTH);
	currentBuffer[numBytesWritten] = '\0'; // ensure c-string

    index += 1;     // Move to next buffer for next function call
    if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;

    return currentBuffer;
}

void StringBuilder::AppendFormat(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
	s32 numBytesWritten = 0;
	const char* formattedTmpBuf = InternalStringFormat(fmt, &args, numBytesWritten);
    va_end(args);
	StringView stringToAppend = StringView(formattedTmpBuf, numBytesWritten);
	Append(stringToAppend);
	ME_ASSERT(capacity > len);
}

const char *StringFormat(const char *text, ...)
{
    va_list args;
    va_start(args, text);
	s32 numBytesWritten = 0;
	const char* result = InternalStringFormat(text, &args, numBytesWritten);
    va_end(args);
	return result;
}

const char* StringFormatNew(meAllocator* allocator, const char *text, ...)
{
	char backing[MAX_TEXT_BUFFER_LENGTH];
	ME_MEMCLEAR(backing, MAX_TEXT_BUFFER_LENGTH);

    va_list args;
    va_start(args, text);
    stbsp_vsnprintf(backing, MAX_TEXT_BUFFER_LENGTH, text, args);
    va_end(args);

	u64 len = CStringLength(backing);
	const char* result = MEALLOC(allocator, len + 1);
	ME_MEMCPY((void*)result, backing, len + 1);
	return result;
}
