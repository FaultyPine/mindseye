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

bool meSerializeTryReadBinaryHeader(
	meSpan serializedBuffer,
	meSerializedHeader* outHeader = nullptr);

bool meSerializeTryReadHeader(
	meSerializationMode mode,
	meSpan serializedBuffer,
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	meSerializedHeader* outHeader);

meSerializeResult SerializeBlocking(SerializeContext& ctx);

void DeserializeBlocking(DeserializeContext& ctx);

void DeserializeFromFileBlocking(
	StringView file,
	DeserializeContext& ctx);
