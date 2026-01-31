#pragma once

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

	meSerializeResult() = default;
	meSerializeResult(ResultType type) : result(type)
	{}
	operator ResultType() const { return result; }
	operator bool() const { return result == SER_SUCCESS; }
};

meSerializeResult SerializeFromFile(
	StringView file,
	meAllocator* allocator,
	const meTypeDescriptor& typeDescriptor,
	meSpan outBuffer);

meSerializeResult SerializeToTextBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator,
    StringView& outResult);

// allocator - for dynamic allocations needed during deserialization (I.E. strings)
// outBuffer - preallocated buffer to deserialize into for POD data of the structure
meSerializeResult DeserializeFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	meSpan outBuffer);
