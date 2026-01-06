#include "me_serialize.h"

#include "reflector/reflection_types.h"
#include "platform/me_os.h"

meSerializeResult SerializeToTextBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator,
    StringView& outResult)
{
	StringBuilder sb(allocator);
	sb.AppendFormat("version= %d\n", typeDesc.version);
	sb.AppendFormat("type= %s\n", (const char*)typeDesc.name.data);
	char* typeData = (char*)data;
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (field.thisType == nullptr)
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
		sb.AppendFormat("%s= %.*s\n", (const char*)field.name.cstr(), STRING_VAARGS(fieldStr));
	}
	// stringbuilders don't own their data, so it's safe to return the data pointer
	outResult = sb;
    return SER_SUCCESS;
}

meSerializeResult DeserializeFromTextBlocking(
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
		StringView fieldView = inText.OffsetView(fieldPos);
		s32 colonPos = EatCharsOffset(fieldView, STRING_LIT("="), true); // relative to fieldPos, either ":" or "="
		if (colonPos == -1)
		{
			return {};
		}
		colonPos += fieldPos;
		s32 valueStart = colonPos + 1;
		StringView valueView = inText.OffsetView(valueStart);
        s32 lineEnd = EatCharsOffset(valueView, STRING_LIT("\r\n"), true); // relative to valueStart, finding either \r or \n, whichever comes first
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
        return SER_FAILURE;
	}
	StringView versionStr = findFieldValueInText(STRING_LIT("version"));
	s32 version = StringParseInt32(versionStr);
	if (version != typeDesc.version)
	{
		LOG_ERROR("Version mismatch deserializing from text for type %.*s. Expected version %d but got version %d", 
			STRING_VAARGS(typeDesc.name), 
			typeDesc.version,
			version);
        return SER_VERSION_MISMATCH;
	}

	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		ME_ON_SCOPE_EXIT([&bumper, &field]() 
		{
			bumper = bumper.Subspan(field.size);
		});
		StringView fieldStr = findFieldValueInText(field.name);
		if (field.thisType == nullptr || !fieldStr)
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
	return SER_SUCCESS;
}

