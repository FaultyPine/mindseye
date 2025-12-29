#include "me_serialize.h"

#include "external/inicpp.hpp"
#include "reflector/reflection_types.h"
#include "platform/me_os.h"

StringView SerializeToTextBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator)
{
	StringBuilder sb(allocator);
	sb.AppendFormat("version: %d\n", typeDesc.version);
	sb.AppendFormat("type: %s\n", (const char*)typeDesc.name.data);
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
		sb.AppendFormat("%s: %.*s\n", (const char*)field.name.cstr(), STRING_VAARGS(fieldStr));
	}
	// stringbuilders don't own their data, so it's safe to return the data pointer
	return sb;
}

bool DeserializeFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	meSpan outBuffer)
{
	Allocation bumper = outBuffer;
	// searches through the text for "fieldName: value" and returns the value portion
	auto findFieldValueInText = [&](StringView fieldName) -> StringView
	{
		s32 fieldPos = FindInString(inText, fieldName);
		if (fieldPos == -1)
		{
			return {};
		}
		s32 colonPos = FindInString(inText.OffsetView(fieldPos), STRING_LIT(":")); // relative to fieldPos
		if (colonPos == -1)
		{
			return {};
		}
		colonPos += fieldPos;
		s32 valueStart = colonPos + 1;
		s32 lineEnd = FindInString(inText.OffsetView(valueStart), STRING_LIT("\n")); // relative to valueStart
		if (lineEnd == -1)
		{
			lineEnd = inText.len;
		}
		lineEnd += valueStart;
		StringView result = inText.OffsetView(valueStart, lineEnd - valueStart);
		result = EatChars(result, ' ');
		return result;
	};
	StringView typeStr = findFieldValueInText(STRING_LIT("type"));
	if (typeStr != StringView(typeDesc.name))
	{
		LOG_ERROR("Type mismatch deserializing from text. Expected %.*s but got %.*s", 
			STRING_VAARGS(typeDesc.name), 
			STRING_VAARGS(typeStr));
	}
	StringView versionStr = findFieldValueInText(STRING_LIT("version"));
	s32 version = StringParseInt32(versionStr);
	if (version != typeDesc.version)
	{
		LOG_ERROR("Version mismatch deserializing from text for type %.*s. Expected version %d but got version %d", 
			STRING_VAARGS(typeDesc.name), 
			typeDesc.version,
			version);
	}

	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		ME_ON_SCOPE_EXIT([&bumper, &field]() 
		{
			bumper = bumper.Subspan(field.size);
		});
		StringView fieldStr = findFieldValueInText(field.name);
		if (field.underlyingType == nullptr || !fieldStr)
		{
			// for reflected fields that don't have entries in the ini,
			// leave them as-is. This way, the caller can default-initialize the structure and
			// fields not in the ini will stay as their defaults.
			continue;
		}
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
