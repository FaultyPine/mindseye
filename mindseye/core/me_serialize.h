#pragma once

enum meSerializeResult
{
    SER_VERSION_MISMATCH,
    SER_SUCCESS,
    SER_FAILURE,
};

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
