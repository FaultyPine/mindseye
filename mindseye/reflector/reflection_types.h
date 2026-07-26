#pragma once

#include "core/me_defines.h"
#include "core/me_memory.h"
#include "core/me_string.h"
#include "core/containers/dynarray.h"
struct meTypeDescriptor;
struct meSerializeResult;
struct meChunker;
struct MAID;

enum meSerializationMode
{
	meSerializationMode_Text,
	meSerializationMode_Binary,
};

struct DeserializeContext
{
	meSerializationMode mode = (meSerializationMode)-1;
	const meTypeDescriptor* typeDesc = nullptr;
	// data to be deserialized, I.E. a string like "0.1" or equivalent
	meSpan sourceData = {};
	// POD, preallocated before deserialization functions are called
	meSpan outputData = {};
	meChunker* chunker = nullptr;
	// external pointer buffer, allocated inside deserialization funcs with the following allocator
	meSpan outputDataExternal = {};
	meAllocator* externalDataAllocator = {};
	// for templated types, this is can be used to get the template params
	const meTypeDescriptor* parentType = {};
	meSerializeResult* outResult = nullptr;
};

struct SerializeContext
{
	meSerializationMode mode = (meSerializationMode)-1;
	const meTypeDescriptor* typeDesc = nullptr;
	// serializing funcs should allocate the string/extra data using this
	meAllocator* allocator = {};
	// buffer that should be serialized
	meSpan sourceData = {};
	// the output serialized buffer
	meOwningSpan serializedData = {};
	meChunker* chunker = nullptr;
	DynArray<MAID>* assetDependencies = nullptr;
	// for templated types, this is can be used to get the template params
	const meTypeDescriptor* parentType = {};
};

struct EditorRenderContext
{
    u8* data = nullptr;
};

// type flags bitfield
typedef s32 meTypeDescriptorFlags;
#define DECLARE_METYPEDESCRIPTOR_FLAGS \
X(ExternalPtr)\
X(ConstantArray)\
X(PaddingMember)\
X(Excluded)\
X(NonSerializedFlagsMarker)\
X(IncludeInGeneratedHeader)\
X(IntegralStub)

#define meTypeDescriptorFlagsSerializedBitmask \
	(~((~0) << meTypeDescriptorFlag_NonSerializedFlagsMarker))

enum meTypeDescriptorFlag_
{
	#define X(name) meTypeDescriptorFlag_##name,
	DECLARE_METYPEDESCRIPTOR_FLAGS
	#undef X
};

StringView meTypeDescriptorFlagToString(meTypeDescriptorFlags flag);

typedef bool(*SerializerFn)(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext& ctx);

typedef bool(*DeserializerFn)(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx);

typedef bool (*EditorRenderFn)(
    EditorRenderContext& ctx);

typedef void (*SetToDefaults)(void* objData);

typedef bool (*EqualsFn)(const meTypeDescriptor& td, const void* a, const void* b);

struct DestroyContext
{
	void* data = nullptr;
	meAllocator* allocator = nullptr;
	const meTypeDescriptor* parentType = nullptr;
};

typedef void (*DestroyFn)(const meTypeDescriptor& td, DestroyContext& ctx);

struct DeepCopyContext
{
	const void* srcData = nullptr;
	meSpan outputData = {};
	meAllocator* allocator = nullptr;
	const meTypeDescriptor* parentType = nullptr;
};

typedef void (*DeepCopyFn)(const meTypeDescriptor& td, DeepCopyContext& ctx);

// Opaque key that identifies an element within a container.
// index for arrays, hash/id for maps, etc
typedef u64 meContainerKey;

typedef void (*meTypeIterateElementFn)(void* elemPtr, const meTypeDescriptor* elemType, meContainerKey elemKey, void* userData);

// calls visitor once per logical element
// fieldDesc is the FIELD descriptor (carries templatedTypes etc.) NOT the global type descriptor (e.g. TD_DYNARRAY)
typedef void (*meTypeIterateContentFn)(void* containerPtr, const meTypeDescriptor* fieldDesc,
                                       meTypeIterateElementFn visitor, void* userData);

// Push one element (pre-allocated and default-constructed by the caller) to the end of the container.
typedef void (*meTypePushElementFn)(void* containerPtr, const meTypeDescriptor* fieldDesc, void* elemData);

// Remove the element identified by key from the container.
typedef void (*meTypeRemoveElementFn)(void* containerPtr, const meTypeDescriptor* fieldDesc, meContainerKey key);

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
	SerializerFn serializerFn = nullptr;
	DeserializerFn deserializerFn = nullptr;
    // compare two buffers of this type for semantic equality
    EqualsFn equalsFn = nullptr;
    EditorRenderFn editorRenderFn = nullptr;
    // invoke default constructor on an arbitrary buffer
    SetToDefaults setToDefaultsFn = nullptr;
	DestroyFn destroyFn = nullptr;
	DeepCopyFn deepCopyFn = nullptr;
    // if this is valid, that implies this type is an iterable container
    meTypeIterateContentFn iterateContentFn = nullptr;
    meTypePushElementFn pushElementFn = nullptr;
    meTypeRemoveElementFn removeElementFn = nullptr;

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

	bool ShouldSerialize() const
	{
		return !(TEST_BIT(flags, meTypeDescriptorFlag_Excluded) || TEST_BIT(flags, meTypeDescriptorFlag_PaddingMember));
	}
};

struct meTypeDescriptorMember
{
	const meTypeDescriptor& field;
	void* data = nullptr;
	const meTypeDescriptor* parentType = nullptr;
	meContainerKey key = 0;
	u64 index = 0;
	u64 offsetBytes = 0;
};

inline u64 meTypeDescriptorMemberOffsetBytes(const meTypeDescriptor& field)
{
	ME_ASSERT(field.offsetBits >= 0);
	ME_ASSERT(field.offsetBits % 8 == 0);
	return (u64)field.offsetBits / 8;
}

inline const meTypeDescriptor* meTypeDescriptorGetSingleTemplateArg(const meTypeDescriptor& fieldDesc)
{
	ME_ASSERT(fieldDesc.templatedTypes && fieldDesc.templatedTypes.size == 1);
	return fieldDesc.templatedTypes[0];
}

template <typename Fn>
bool meTypeDescriptorWalkMembers(
	const meTypeDescriptor& typeDesc,
	void* data,
	Fn&& fn,
	bool serializableOnly = true)
{
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if ((serializableOnly && !field.ShouldSerialize()) || field.thisType == nullptr)
		{
			continue;
		}

		u64 offsetBytes = meTypeDescriptorMemberOffsetBytes(field);
		meTypeDescriptorMember member = { field, data ? (u8*)data + offsetBytes : nullptr, &typeDesc, i, i, offsetBytes };
		if (!fn(member))
		{
			return false;
		}
	}
	return true;
}

template <typename Fn>
bool meTypeDescriptorWalkElements(
	const meTypeDescriptor& typeDesc,
	void* data,
	Fn&& fn,
	const meTypeDescriptor* parentType = nullptr)
{
	if (typeDesc.thisType && TEST_BIT(typeDesc.flags, meTypeDescriptorFlag_ConstantArray))
	{
		if (typeDesc.thisType->size == 0)
		{
			return true;
		}

		u32 count = typeDesc.size / typeDesc.thisType->size;
		for (u32 i = 0; i < count; i++)
		{
			meTypeDescriptorMember element = {
				*typeDesc.thisType,
				data ? (u8*)data + (typeDesc.thisType->size * i) : nullptr,
				&typeDesc,
				i,
				i,
				0,
			};
			if (!fn(element))
			{
				return false;
			}
		}
		return true;
	}

	const meTypeDescriptor* containerType = typeDesc.iterateContentFn ? &typeDesc : typeDesc.thisType;
	if (!containerType || !containerType->iterateContentFn)
	{
		return false;
	}

	const meTypeDescriptor* fieldDesc = parentType ? parentType : &typeDesc;
	using FnType = typename remove_reference<Fn>::type;
	struct WalkCtx
	{
		FnType* fn = nullptr;
		const meTypeDescriptor* parentType = nullptr;
		bool keepWalking = true;
		u64 index = 0;
	};
	WalkCtx walkCtx = { &fn, fieldDesc };
	containerType->iterateContentFn(
		data,
		fieldDesc,
		+[](void* elemPtr, const meTypeDescriptor* elemType, meContainerKey elemKey, void* userData)
		{
			WalkCtx& ctx = *(WalkCtx*)userData;
			if (!ctx.keepWalking)
			{
				return;
			}

			meTypeDescriptorMember element = { *elemType, elemPtr, ctx.parentType, elemKey, ctx.index++, 0 };
			ctx.keepWalking = (*ctx.fn)(element);
		},
		&walkCtx);
	return walkCtx.keepWalking;
}


extern meTypeDescriptor TD_UNSIGNED_INT;
extern meTypeDescriptor TD_INT;
extern meTypeDescriptor TD_UNSIGNED_SHORT;
extern meTypeDescriptor TD_SHORT;
extern meTypeDescriptor TD_UNSIGNED_LONG;
extern meTypeDescriptor TD_LONG;
extern meTypeDescriptor TD_LONG_LONG;
extern meTypeDescriptor TD_UNSIGNED_LONG_LONG;
extern meTypeDescriptor TD_FLOAT;
extern meTypeDescriptor TD_DOUBLE;
extern meTypeDescriptor TD_BOOL;
extern meTypeDescriptor TD_CHAR;
extern meTypeDescriptor TD_UNSIGNED_CHAR;
extern meTypeDescriptor TD_WCHAR_T;
extern meTypeDescriptor TD_VEC3;
extern meTypeDescriptor TD_QUAT;
extern meTypeDescriptor TD_SPAN;
extern meTypeDescriptor TD_STRINGVIEW;
extern meTypeDescriptor TD_STRING;

// Serializer/Deserializer functions for sized buffer types
bool sizedBufferSerializer(const meTypeDescriptor&, SerializeContext& ctx);
bool sizedBufferDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
bool stringSerializer(const meTypeDescriptor&, SerializeContext& ctx);
bool stringDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
bool primitiveSerializer(const meTypeDescriptor&, SerializeContext& ctx);
bool primitiveDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
bool serializationDisallowed(const meTypeDescriptor&, SerializeContext& ctx);
bool deserializationDisallowed(const meTypeDescriptor&, DeserializeContext& ctx);
bool sizedBufferEquals(const meTypeDescriptor& td, const void* a, const void* b);
void sizedBufferDestroy(const meTypeDescriptor& td, DestroyContext& ctx);
void stringDestroy(const meTypeDescriptor& td, DestroyContext& ctx);
void sizedBufferDeepCopy(const meTypeDescriptor& td, DeepCopyContext& ctx);
void stringDeepCopy(const meTypeDescriptor& td, DeepCopyContext& ctx);

// Returns true if two buffers are semantically equal as this reflected type.
// parentType is used for templated types.
bool meFieldsEqual(
	const meTypeDescriptor& typeDesc,
	const void* a,
	const void* b,
	const meTypeDescriptor* parentType = nullptr);

// Frees owned backing data inside a reflected type. Primitive/POD fields are no-ops.
void meTypeDescriptorDestroy(
	const meTypeDescriptor& typeDesc,
	DestroyContext& ctx);

// Deep-copies a reflected type, allocating owned backing data through ctx.allocator.
void meTypeDescriptorDeepCopy(
	const meTypeDescriptor& typeDesc,
	DeepCopyContext& ctx);

// NOTE: there are static maps mapping between reflected types and their type descriptors
// in me_reflector.cpp
// I.E. "String" -> TD_STRING or "glm::vec3<3, float>" -> TD_VEC3


template <typename T>
void meTypeDescriptorSetToDefaults(void* objData)
{
    new (objData) T();
}

template <typename T>
bool meTypeDescriptorEquals(
    const meTypeDescriptor& td, 
    const void* a, 
    const void* b)
{
    constexpr bool isPOD = !std::is_same_v<T, void> && std::is_trivially_destructible_v<T> &&
        //std::is_trivial_v<T> && // not using this check, because it flags types that use unions as nontrivial. In this situation, that's fine.
        std::is_standard_layout_v<T>;
    if constexpr (isPOD && requires(const T& x, const T& y)
    { 
        x == y;
    })
    {
        return (*(const T*)a) == (*(const T*)b);
    }
    if (td.thisType && TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray))
    {
        u32 elemSize = td.thisType->size;
        if (elemSize == 0) return ME_MEMCMP(a, b, td.size) == 0;
        bool elementsEqual = true;
        meTypeDescriptorWalkElements(td, const_cast<void*>(a),
            [&](const meTypeDescriptorMember& element)
            {
                const void* bElem = (const u8*)b + (elemSize * element.index);
                if (!meTypeDescriptorEquals<void>(element.field, element.data, bElem))
                {
                    elementsEqual = false;
                    return false;
                }
                return true;
            });
        if (!elementsEqual) return false;
        return true;
    }
    else if (td.thisType)
    {
        if (td.thisType->equalsFn)
        {
            return td.thisType->equalsFn(td, a, b);
        }
        return meTypeDescriptorEquals<void>(*td.thisType, a, b);
    }
    if constexpr (std::is_same_v<T, void>)
    {
        if (td.equalsFn)
        {
            return td.equalsFn(td, a, b);
        }
    }
    if (td.fields.size > 0)
    {
        bool fieldsEqual = true;
        meTypeDescriptorWalkMembers(td, const_cast<void*>(a),
            [&](const meTypeDescriptorMember& member)
            {
                const void* bField = (const u8*)b + member.offsetBytes;
                if (!meTypeDescriptorEquals<void>(member.field, member.data, bField))
                {
                    fieldsEqual = false;
                    return false;
                }
                return true;
            });
        if (!fieldsEqual) return false;
        return true;
    }
    // NOTE: by having this here it means structures can't use custom equalsFn, which i think is fine.
    else if (td.equalsFn)
    {
        return td.equalsFn(td, a, b);
    }
    if constexpr (isPOD)
    {
        // fallback to memcmp
        return ME_MEMCMP(a, b, td.size) == 0;
    }
    // if a type descriptor was made without an equalsFn that also isn't POD, 
    // you need to implement the equalsFn or make it pod. See DynArray's equalsFn for reference
    ME_ASSERT(false);
    return false;
}
