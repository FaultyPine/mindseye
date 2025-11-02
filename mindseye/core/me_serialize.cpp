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

bool DeserializeFromIniBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inFilename,
	meSpan outBuffer)
{
	ME_ASSERT(meOSFileExists(inFilename));
	inicpp::IniManager iniObj(inFilename.data);
	Allocation bumper = outBuffer;
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		ME_ON_SCOPE_EXIT([&bumper, &field]() 
		{
			bumper = bumper.Subspan(field.size);
		});
		std::string fieldStdStr = iniObj[typeDesc.name.cstr()].toString(field.name.data);
		if (field.underlyingType == nullptr || fieldStdStr.empty())
		{
			// for reflected fields that don't have entries in the ini,
			// leave them as-is. This way, the caller can default-initialize the structure and
			// fields not in the ini will stay as their defaults.
			continue;
		}
		StringView fieldStr = StringView(fieldStdStr.c_str(), fieldStdStr.size());
		fieldStr = StringTrim(fieldStr, STRING_LIT("\""));
		DeserializeContext ctx = {};
		ctx.inputData = fieldStr.ToSpan();
		ctx.outputData = bumper;
		ctx.externalDataAllocator = allocator;
		field.FromString(ctx);
	}
	// NOTE: padding is relevant here...
	ME_ASSERT(outBuffer.size == typeDesc.size);
	return true;
}
