#pragma once

#include "core/me_defines.h"
#include "core/containers/dynarray.h"
#include "core/me_memory.h"
#include "core/me_string.h"

struct DeserializeContext
{
	// data to be deserialized
	meSpan inputData = {};

	// POD, preallocated before deserialization functions are called
	meSpan outputData = {}; 
	// external pointer buffer, allocated inside deserialization funcs with the following allocator
	meSpan outputDataExternal = {}; 
	meAllocator* externalDataAllocator = {};
};

typedef s32 meTypeID;

// type flags bitfield
typedef s32 meTypeDescriptorFlag;
enum meTypeDescriptorFlag_
{
	meTypeDescriptorFlag_ExternalPtr,
};

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

	typedef StringView(*serializerToString)(meAllocator* allocator, meSpan data);
	typedef bool(*deserializerFromString)(DeserializeContext& ctx);

	// for non-pod types, these can be assigned and will
	// be called instead of default primitive serialization funcs
	serializerToString strSerializer = nullptr;
	deserializerFromString strDeserializer = nullptr;

	StringView ToString(meAllocator* allocator, meSpan data) const;
	bool FromString(DeserializeContext& ctx) const;

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
extern meTypeDescriptor TD_VEC3;
extern meTypeDescriptor TD_SPAN;
extern meTypeDescriptor TD_STRINGVIEW; // basically the same as span
extern meTypeDescriptor TD_STRING;

// NOTE: there are static maps mapping between reflected types and their type descriptors
// in me_reflector.cpp
// I.E. "String" -> TD_STRING or "glm::vec3<3, float>" -> TD_VEC3


