#pragma once

#include "asset/me_asset.h"

struct meSerializeResult
{
	enum ResultType
	{
		SER_FAILURE,
		SER_VERSION_MISMATCH,
		SER_SUCCESS,
	};
	ResultType result = SER_FAILURE;
	u32 serializedUniqueIdentifier = 0;
	// The MAID of the asset currently being deserialized. 
	// Zeroed / meaningless for Serialize* calls and deserializations with no owner.
	MAID ownerMaid = {};

	meSerializeResult() = default;
	meSerializeResult(ResultType type) : result(type)
	{}
	operator ResultType() const { return result; }
	operator bool() const { return result == SER_SUCCESS; }
};


meSerializeResult SerializeToTextBlocking(
	const meTypeDescriptor& typeDesc,
	void* data,
	meAllocator* allocator,
	StringView& outResult);

void DeserializeFromFileBlocking(
	StringView file,
	meAllocator* allocator,
	const meTypeDescriptor& typeDescriptor,
	meSpan outBuffer,
	meSerializeResult& outResult);

// allocator  - for dynamic allocations needed during deserialization (I.E. strings)
// outBuffer  - preallocated buffer to deserialize into for POD data of the structure
void DeserializeFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	meSpan outBuffer,
	meSerializeResult& outResult);


// Returns true if the two buffers, interpreted as the given type, are
// semantically equal
// parentType is used for templated types (see meTypeDescriptor docs).
bool meFieldsEqual(
	const meTypeDescriptor& typeDesc,
	const void* a,
	const void* b,
	const meTypeDescriptor* parentType = nullptr);

// Deep-copies a buffer described by typeDesc, allocating any owned backing data
// through ctx.allocator.
void meTypeDescriptorDeepCopy(
	const meTypeDescriptor& typeDesc,
	DeepCopyContext& ctx);

// Like SerializeToTextBlocking, but only emits fields whose values
// differ from those in templateData. Header MAID is always written.
meSerializeResult SerializeOverridesToTextBlocking(
	const meTypeDescriptor& assetTypeDesc,
	void* instanceData,
	void* templateData,
	meAllocator* allocator,
	StringView& outResult);

// Counterpart: copies templateData into outBuffer first, then
// overwrites any fields present in inText.
void DeserializeOverridesFromTextBlocking(
	const meTypeDescriptor& assetTypeDesc,
	meAllocator* allocator,
	StringView inText,
	const void* templateData,
	meSpan outBuffer,
	meSerializeResult& outResult);


void sizedBufferSerializer(
    const meTypeDescriptor& typedescriptor,
    SerializeContext& ctx);

void stringSerializer(
    const meTypeDescriptor& typedescriptor,
    SerializeContext& ctx);
