#pragma once

StringView SerializeToTextBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator);

// allocator - for dynamic allocations needed during deserialization (I.E. strings)
// outBuffer - preallocated buffer to deserialize into for POD data of the structure
bool DeserializeFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	meSpan outBuffer);

void SerializeToIniBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	StringView outFilename);

// allocator - for dynamic allocations needed during deserialization (I.E. strings)
// outBuffer - preallocated buffer to deserialize into for POD data of the structure
bool DeserializeFromIniBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inFilename,
	meSpan outBuffer);