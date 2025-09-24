#pragma once

#include "core/me_defines.h"
#include "core/containers/dynarray.h"
#include "core/me_memory.h"

#include <string_view> // C++17 for std::string_view

constexpr std::string_view::size_type constexpr_strstr(
	std::string_view haystack, 
	std::string_view needle) noexcept 
{
	if (needle.empty()) 
	{
		return 0; 
	}
	for (std::string_view::size_type i = 0; i + needle.length() <= haystack.length(); ++i) 
	{
		if (haystack.substr(i, needle.length()) == needle) {
			return i;
		}
	}
	return std::string_view::npos;
}

typedef s32 meTypeID;

struct meTypeDescriptor
{
	String name = {};
	String editorName = {};
	String tooltip = {};
	meSpanTyped<meTypeDescriptor> fields = {};
	s32 value = 0;
	s32 flags = 0;
	s32 version = 0;
	
	u32 size = 0;
	u32 align = 0;
	s32 offsetBits = 0;

	meTypeDescriptor* underlyingType = nullptr;

	StringView ToString(meAllocator* allocator, meSpan data) const;
	meSpan FromString(meAllocator* allocator, StringView str) const;

	bool operator==(const meTypeDescriptor& other) const
	{
		return name == other.name &&
			size == other.size &&
			offsetBits == other.offsetBits &&
			align == other.align && 
			flags == other.flags && 
			underlyingType == other.underlyingType;
	}
};

// TODO: relative? fixup?....
struct meSerializedPtr
{
	void* ptr;
};

extern meTypeDescriptor TD_UNSIGNED_INT;
extern meTypeDescriptor TD_INT;
extern meTypeDescriptor TD_UNSIGNED_SHORT;
extern meTypeDescriptor TD_SHORT;
extern meTypeDescriptor TD_UNSIGNED_LONG;
extern meTypeDescriptor TD_LONG;
extern meTypeDescriptor TD_LONGLONG;
extern meTypeDescriptor TD_UNSIGNED_LONG_LONG;
extern meTypeDescriptor TD_FLOAT;
extern meTypeDescriptor TD_DOUBLE;
extern meTypeDescriptor TD_BOOL;
extern meTypeDescriptor TD_CHAR;
extern meTypeDescriptor TD_UNSIGNED_CHAR;
extern meTypeDescriptor TD_WCHAR;
extern meTypeDescriptor TD_POINTER;
