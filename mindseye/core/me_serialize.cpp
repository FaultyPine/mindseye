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
		if (field.offsetBits % 8 != 0)
		{
			UNIMPLEMENTED(); // TODO
		}
		u32 offsetBytes = field.offsetBits / 8;
		meSpan fieldData = meSpan(typeData + offsetBytes, field.size);
		StringView fieldStr = field.ToString(GetTLScratch(), fieldData);
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
			ME_MEMCLEAR(bumper.data, field.size);
			bumper = bumper.Subspan(field.size);
			continue;
		}
		std::string fieldStdStr = iniObj[typeDesc.name.cstr()].toString(field.name.data);
		StringView fieldStr = StringView(fieldStdStr.c_str(), fieldStdStr.size());
		fieldStr = StringTrim(fieldStr, STRING_LIT("\""));
		DeserializeContext ctx = {};
		ctx.inputData = fieldStr.ToSpan();
		ctx.outputData = MEALLOC(scratch, field.size);
		ctx.externalDataAllocator = allocator;
		field.FromString(ctx);
		ME_MEMCPY(bumper.data, ctx.outputData.data, ctx.outputData.size);
		bumper = bumper.Subspan(ctx.outputData.size);
	}
	// not including "external" data, which was already allocated with our passed-in allocator
	u64 deserializedPODSize = bumper.data - scratchWorkMem.data;
	// NOTE: keep in mind that the "external" data is allocated before this is
	Allocation resultMemory = MEALLOC(allocator, deserializedPODSize);
	ME_MEMCPY(resultMemory, scratchWorkMem, deserializedPODSize);
	return resultMemory;
}

