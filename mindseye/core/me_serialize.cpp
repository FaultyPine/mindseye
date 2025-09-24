#include "me_serialize.h"

#include "external/inicpp.hpp"
#include "reflector/reflection_types.h"

// TODO: this inicpp library is not good. 
// It works, so i'm using it to stand up the rest of the infra here
// Swap it out asap.

void SerializeToIniBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator,
	StringView outFilename)
{
	inicpp::IniManager iniObj(outFilename.data);
	iniObj.set("type", (const char*)typeDesc.name.data);
	iniObj.set("version", typeDesc.version);
	char* typeData = (char*)data;
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		u32 offsetBytes = field.offsetBits / 8;
		meSpan fieldData = meSpan(typeData + offsetBytes, field.size);
		// Doing this kind of textual human-readable serialization
		// requires a heavy ToString call. Do I want to use human readable
		// serialization formats???? Not sure what I really want to do here...
		StringView fieldStr = field.ToString(allocator, fieldData);
		iniObj["members"][(const char*)field.name.data] = (const char*)fieldStr.data;
	}
}

meSpan DeserializeFromIniBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inFilename)
{
	inicpp::IniManager iniObj(inFilename.data);
	Allocation deserializationMemory = MEALLOC(allocator, MEGABYTES_BYTES(5));
	Allocation bumper = deserializationMemory;
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		// Doing this kind of textual human-readable serialization
		// requires a heavy ToString call. Do I want to use human readable
		// serialization formats???? Not sure what I really want to do here...
		std::string fieldStr = iniObj["members"].toString(field.name.data);
		meSpan fieldData = field.FromString(allocator, StringView(fieldStr.c_str(), fieldStr.size())); // TODO
		ME_MEMCPY(bumper.data, fieldData.data, fieldData.size);
		bumper = bumper.Subspan(fieldData.size);
	}
	return deserializationMemory;
}

