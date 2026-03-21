#pragma once

#include "core/me_defines.h"
#include "core/me_memory.h"
#include "core/me_string.h"
struct meTypeDescriptor;

// TODO: instead of a "to string serialization" and equiv deserialization
// function, just have 1 serialize and 1 deserialize per type
// and in the ctxs, have an enum like "SERIALIZE_KIND_TEXT" "SERIALIZE_KIND_BINARY"

struct DeserializeContext
{
	// data to be deserialized, I.E. a string like "0.1" or equivalent
	meSpan inputData = {};
	// POD, preallocated before deserialization functions are called
	meSpan outputData = {}; 
	// external pointer buffer, allocated inside deserialization funcs with the following allocator
	meSpan outputDataExternal = {}; 
	meAllocator* externalDataAllocator = {};
	// for templated types, this is can be used to get the template params
	const meTypeDescriptor* parentType = {};
};

struct SerializeContext
{
	// serializing funcs should allocate the string/extra data using this
	meAllocator* allocator = {};
	// buffer that should be serialized
	meSpan data = {};
	// for templated types, this is can be used to get the template params
	const meTypeDescriptor* parentType = {};
};

// type flags bitfield
typedef s32 meTypeDescriptorFlags;
#define DECLARE_METYPEDESCRIPTOR_FLAGS \
X(ExternalPtr)\
X(ConstantArray)\
X(PaddingMember)\
X(Excluded)\
X(NonSerializedFlagsMarker)\
X(IncludeInGeneratedHeader)

#define meTypeDescriptorFlagsSerializedBitmask \
	(~((~0) << meTypeDescriptorFlag_NonSerializedFlagsMarker))

enum meTypeDescriptorFlag_
{
	#define X(name) meTypeDescriptorFlag_##name,
	DECLARE_METYPEDESCRIPTOR_FLAGS
	#undef X
};

StringView meTypeDescriptorFlagToString(meTypeDescriptorFlags flag);

typedef StringView(*SerializerToStringFn)(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext ctx);
typedef bool(*DeserializerFn)(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx);

struct meTypeDescriptor
{
	String name = {};
	String editorName = {};
	String tooltip = {};
	meSpanTyped<meTypeDescriptor> fields = {};
	s32 value = 0;
	meTypeDescriptorFlags flags = 0;
	s32 version = 0;
	
	// in bytes
	u32 size = 0;
	u32 align = 0;
	// in bits! to account for possible bitfield members
	s32 offsetBits = 0;

	meTypeDescriptor* thisType = nullptr;
	// it's a bit funky, because if thisType is a templated type
	// thisType *alone* is not enough to determine that is it templated
	// What this means is if you have a field like
	// DynArray<int> someInts;
	// the type descriptor will look like
	// g_templatedStuff = { DT_INT }
	// {name = someInts, thisType = DT_DYNARRAY, templatedTypes = g_templatedStuff };
	// so to get the full context of the "type of someInts"
	// you can't just look at thisType
	// The reason for this is that a meTypeDescriptor* isn't really an "instance" of a type descriptor
	// it's a pointer to some static definition of one. So we (could, but) don't instantiate multiple DYNARRAY type descriptors per template args permutation
	meSpanTyped<meTypeDescriptor*> templatedTypes = {};

	// for non-pod types, these can be assigned and will
	// be called instead of default primitive serialization funcs
	SerializerToStringFn strSerializer = nullptr;
	DeserializerFn strDeserializer = nullptr;

	bool operator==(const meTypeDescriptor& other) const
	{
		return name == other.name &&
			size == other.size &&
			offsetBits == other.offsetBits &&
			align == other.align && 
			flags == other.flags && 
			*thisType == *other.thisType;
	}
	void CopyFrom(const meTypeDescriptor& other)
	{
		name = other.name;
		editorName = other.editorName;
		tooltip = other.tooltip;
		for (u32 i = 0; i < other.fields.size; i++)
		{
			fields[i].CopyFrom(other.fields[i]);
		}
		value = other.value;
		flags = other.flags;
		version = other.version;
	
		size = other.size;
		align = other.align;
		offsetBits = other.offsetBits;

		thisType = other.thisType;
	}

	bool ShouldSerializeText() const
	{
		return !(TEST_BIT(flags, meTypeDescriptorFlag_Excluded) || TEST_BIT(flags, meTypeDescriptorFlag_PaddingMember));
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
extern meTypeDescriptor TD_QUAT;
extern meTypeDescriptor TD_SPAN;
extern meTypeDescriptor TD_STRINGVIEW; // basically the same as span
extern meTypeDescriptor TD_STRING;

// Serializer/Deserializer functions for sized buffer types
StringView sizedBufferSerializer(const meTypeDescriptor&, SerializeContext ctx);
bool sizedBufferDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
bool stringDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);

// NOTE: there are static maps mapping between reflected types and their type descriptors
// in me_reflector.cpp
// I.E. "String" -> TD_STRING or "glm::vec3<3, float>" -> TD_VEC3

