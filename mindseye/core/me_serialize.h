#pragma once



void SerializeToIniBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator,
	StringView outFilename);

meSpan DeserializeFromIniBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inFilename);