#include "me_serialize.h"

#include "external/inicpp.hpp"
#include "reflector/reflection_types.h"
#include "platform/me_os.h"

// TODO: this inicpp library is not good. 
// It works, so i'm using it to stand up the rest of the infra here
// Swap it out asap.

void SerializeToIniBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	StringView outFilename)
{
	{
		OSFileReference outExistingFile;
		outExistingFile.InitWithoutOpening(outFilename);
		meOSFileDelete(outExistingFile);
	}
	inicpp::IniManager iniObj(outFilename.data);
	iniObj.set("type", (const char*)typeDesc.name.data);
	iniObj.set("version", typeDesc.version);
	char* typeData = (char*)data;
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (field.underlyingType == nullptr)
		{
			continue;
		}
		u32 offsetBytes = field.offsetBits / 8;
		meSpan fieldData = meSpan(typeData + offsetBytes, field.size);
		StringView fieldStr = field.ToString(GetTLScratch(), fieldData);
		// Doing this kind of textual human-readable serialization
		// requires a heavy ToString call. Do I want to use human readable
		// serialization formats???? Not sure what I really want to do here...
		iniObj[typeDesc.name.cstr()][(const char*)field.name.cstr()] = (const char*)fieldStr.cstr();
	}
}

meSpan DeserializeFromIniBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inFilename)
{
	ME_ASSERT(meOSFileExists(inFilename));
	inicpp::IniManager iniObj(inFilename.data);
	meAllocator* scratch = GetTLScratch();
	Allocation scratchWorkMem = MEALLOC(scratch, MEGABYTES_BYTES(1));
	Allocation bumper = scratchWorkMem;
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (field.underlyingType == nullptr)
		{
			bumper = bumper.Subspan(field.size);
			continue;
		}
		std::string fieldStr = iniObj[typeDesc.name.cstr()].toString(field.name.data);
		meSpan fieldData = field.FromString(scratch, StringView(fieldStr.c_str(), fieldStr.size()));
		// Doing this kind of textual human-readable serialization
		// requires a heavy ToString call. Do I want to use human readable
		// serialization formats???? Not sure what I really want to do here...
		ME_MEMCPY(bumper.data, fieldData.data, fieldData.size);
		bumper = bumper.Subspan(fieldData.size);
	}
	u64 deserializedSize = scratchWorkMem.size - bumper.size;
	Allocation resultMemory = MEALLOC(allocator, deserializedSize);
	ME_MEMCPY(resultMemory, scratchWorkMem, deserializedSize);
	return resultMemory;
}

