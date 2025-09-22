#pragma once



void SerializeToIniBlocking(
	const meTypeDescriptor& typeDesc, 
	const void* data,
	StringView outFilename);

