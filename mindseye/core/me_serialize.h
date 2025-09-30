#pragma once



void SerializeToIniBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	StringView outFilename);

meSpan DeserializeFromIniBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inFilename);