#include "me_serialize.h"

#include "reflector/reflection_types.h"
#include "platform/me_os.h"

meSerializeResult SerializeFromFile(
	StringView filepath,
	meAllocator* allocator,
	const meTypeDescriptor& typeDescriptor,
	meSpan outBuffer)
{
	// when debugging serialization, we might want to open the file being worked with, so we close the file handle before doing the Deserialize call 
	Allocation tempFileContent = {};
	{
		OSFileReference file;
		meOSOpenFile(file, filepath, (OSFileFlags_OnlyIfExists | OSFileFlags_ScopedFile)); // TODO: memmap the file instead
		tempFileContent = MEALLOC(GetTLScratch(), meOSGetFileSize(file));
		meOSReadFileContents(file, tempFileContent, tempFileContent.size);
	}
	
	meSerializeResult res = DeserializeFromTextBlocking(typeDescriptor, allocator, StringView(tempFileContent), outBuffer);
	// TODO: could/should be replaced with timestamp
	res.serializedUniqueIdentifier = HashBytesL((u8*)tempFileContent.data, tempFileContent.size); 
	return res;
}

meSerializeResult SerializeToTextBlocking(
	const meTypeDescriptor& typeDesc, 
	void* data,
	meAllocator* allocator,
    StringView& outResult)
{
	// TODO: also fill in the hash of the result
	StringBuilder sb(allocator);
	sb.AppendFormat("version = %d\n", typeDesc.version);
	sb.AppendFormat("type = %s\n", (const char*)typeDesc.name.data);
	char* typeData = (char*)data;
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (field.thisType == nullptr || !field.ShouldSerializeText())
		{
			continue;
		}
		if (field.offsetBits % 8 != 0)
		{
			UNIMPLEMENTED(); // TODO
		}
		u32 offsetBytes = field.offsetBits / 8;
		meSpan fieldData = meSpan(typeData + offsetBytes, field.size);
		SerializeContext ctx = {};
		ctx.allocator = GetTLScratch();
		ctx.data = fieldData;
		StringView fieldStr = field.ToString(ctx);
		sb.AppendFormat("%s = %.*s\n", (const char*)field.name.cstr(), STRING_VAARGS(fieldStr));
	}
	// stringbuilders don't own their data, so it's safe to return the data pointer
	outResult = sb;
    return meSerializeResult::SER_SUCCESS;
}

meSerializeResult DeserializeFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	meSpan outBuffer)
{
	// TODO: also fill in the hash of the result
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
        return meSerializeResult::SER_FAILURE;
	}
	StringView versionStr = findFieldValueInText(STRING_LIT("version"));
	s32 version = StringParseInt32(versionStr);
	if (version != typeDesc.version)
	{
		LOG_ERROR("Version mismatch deserializing from text for type %.*s. Expected version %d but got version %d", 
			STRING_VAARGS(typeDesc.name), 
			typeDesc.version,
			version);
        return meSerializeResult::SER_VERSION_MISMATCH;
	}

	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		ME_ON_SCOPE_EXIT([&bumper, &field]() 
		{
			bumper = bumper.Subspan(field.size);
		});
		StringView fieldStr = findFieldValueInText(field.name);
		if (field.thisType == nullptr || !fieldStr || !field.ShouldSerializeText())
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
	return meSerializeResult::SER_SUCCESS;
}



// =========================================================


// Delimiter pairs for nested structure tracking during deserialization
// Add new pairs here as needed (e.g., '<', '>' for angle brackets)
struct DelimiterPair { char open; char close; };
static constexpr DelimiterPair g_nestedDelimiters[] = {
	{ '(', ')' },
	{ '[', ']' },
	{ '{', '}' },
};
static constexpr u32 g_nestedDelimiterCount = sizeof(g_nestedDelimiters) / sizeof(g_nestedDelimiters[0]);

// str might look like
// [ 4, "hello", [0, {val=4.5, name="s"}], 0 ]
StringView meDeserializeEatUntilNextElement(
	StringView& str,
	char openDelim,
	char closeDelim,
	char separator)
{
	// Skip leading whitespace
	while (str.len > 0 && IsWhitespace(str.data[0]))
	{
		str = str.OffsetView(1);
	}
	// If we're at the opening delimiter, skip it
	if (str.len > 0 && str.data[0] == openDelim)
	{
		str = str.OffsetView(1);
		while (str.len > 0 && IsWhitespace(str.data[0]))
		{
			str = str.OffsetView(1);
		}
	}
	// If empty or at closing delimiter
	if (str.len == 0 || str.data[0] == closeDelim)
	{
		return {};
	}
	// Find the end of this element
	// Track depth for all delimiter types to handle nested structures
	u32 depths[g_nestedDelimiterCount] = {};
	u32 elementEnd = 0;
	bool foundEnd = false;
	for (u32 i = 0; i < str.len; i++)
	{
		char c = str.data[i];
		// Track all delimiter types
		for (u32 d = 0; d < g_nestedDelimiterCount; d++)
		{
			if (c == g_nestedDelimiters[d].open) depths[d]++;
			else if (c == g_nestedDelimiters[d].close) { if (depths[d] > 0) depths[d]--; }
		}

		bool isOutside = true;
		for (u32 d = 0; d < g_nestedDelimiterCount; d++)
		{
			if (depths[d] > 0) { isOutside = false; break; }
		}

		if (c == closeDelim && isOutside)
		{
			// We've reached the end of the entire list
			elementEnd = i;
			foundEnd = true;
			break;
		}
		else if (c == separator && isOutside)
		{
			// Found separator at top level - this is the end of the current element
			elementEnd = i;
			foundEnd = true;
			break;
		}
	}
	// no delimiter found, take rest of string
	if (!foundEnd)
	{
		elementEnd = str.len;
	}
	// Extract the element
	u32 trimmedEnd = elementEnd;
	while (trimmedEnd > 0 && IsWhitespace(str.data[trimmedEnd-1]))
	{
		trimmedEnd--;
	}
	StringView element = str.OffsetView(0, trimmedEnd);
	// Update str to point past the element and separator
	str = str.OffsetView(elementEnd);
	// Skip the separator if present
	if (str.len > 0 && str.data[0] == separator)
	{
		str = str.OffsetView(1);
	}
	return element;
}
#include <sstream>
StringView meTypeDescriptor::ToString(SerializeContext ctx) const
{
	StringBuilder builder = StringBuilder(ctx.allocator);
	meSpan data = ctx.data;
    // Handle null/empty data
    if (!data.data || data.size == 0 || !ShouldSerializeText()) 
	{
		return {};
    }
    
    // custom override
	if (strSerializer)
	{
		return strSerializer(*this, ctx);
	}
	ctx.parentType = this;

	// append multiple of the inner types for arrays
	if (thisType && TEST_BIT(flags, meTypeDescriptorFlag_ConstantArray))
	{
		builder.Append(STRING_LIT("{ "));
		u32 numArrayElements = size / thisType->size;
		for (u32 i = 0; i < numArrayElements; i++)
		{
			meSpan arrayElement = data.Subspan(thisType->size * i, thisType->size);
			SerializeContext newCtx = ctx;
			newCtx.data = arrayElement;
			StringView arrayElementStr = thisType->ToString(newCtx);
			builder.Append(arrayElementStr);
			(i == numArrayElements - 1) ? void() : builder.Append(STRING_LIT(", "));
		}
		builder.Append(STRING_LIT("}"));
		return builder;
	}

    // If this is a primitive type with an underlying type, delegate to it
    if (thisType && fields.size == 0) 
	{
        return thisType->ToString(ctx);
    }

    // Handle primitive types based on name and size
    if (fields.size == 0) 
	{
        if (this == &TD_INT) 
		{
			builder.AppendFormat("%d", *((s32*)data.data));
        }
        else if (this == &TD_UNSIGNED_INT) 
		{
            builder.AppendFormat("%u", *((u32*)data.data));
        }
        else if (this == &TD_LONGLONG) 
		{
            builder.AppendFormat("%lld", *((s64*)data.data));
        }
        else if (this == &TD_UNSIGNED_LONG_LONG) 
		{
            builder.AppendFormat("%llu", *((u64*)data.data));
        }
        else if (this == &TD_SHORT) 
		{
            builder.AppendFormat("%d", (s32)*((s16*)data.data));
        }
        else if (this == &TD_UNSIGNED_SHORT) 
		{
            builder.AppendFormat("%u", (u32)*((u16*)data.data));
        }
        else if (this == &TD_CHAR) 
		{
            builder.AppendFormat("%d", (s32)*((s8*)data.data));
        }
        else if (this == &TD_UNSIGNED_CHAR) 
		{
            builder.AppendFormat("%u", (u32)*((u8*)data.data));
        }
        else if (this == &TD_FLOAT) 
		{
            builder.AppendFormat("%.6f", *((float*)data.data));
        }
        else if (this == &TD_DOUBLE) 
		{
            builder.AppendFormat("%.15f", *((double*)data.data));
        }
        else if (this == &TD_BOOL) 
		{
            builder.AppendFormat("%s", *((bool*)data.data) ? "true" : "false");
        }
		else if (this == &TD_STRINGVIEW)
		{
			builder.Append(StringView((const char*)data.data, data.size));
		}
        else if (this == &TD_VEC3)
        {
            float* vecData = (float*)data.data;
            builder.AppendFormat("(%.6f %.6f %.6f)", vecData[0], vecData[1], vecData[2]);
        }
		else if (this == &TD_QUAT) // 4 component vector & quat are the same
		{
            float* vecData = (float*)data.data;
			builder.AppendFormat("(%.6f %.6f %.6f %.6f)", vecData[0], vecData[1], vecData[2], vecData[3]);
		}
        else 
		{
			UNIMPLEMENTED();
        }
    }
	else
	{
        builder.Append(STRING_LIT("{ "));
		for (u64 i = 0; i < fields.size; i++)
        {
            const meTypeDescriptor& field = fields[i];
            ME_ASSERT(field.offsetBits % 8 == 0);
			meSpan nextField = meSpan(data.data + (field.offsetBits / 8), field.size);
			SerializeContext newCtx = ctx;
			newCtx.data = nextField;
            StringView stringedField = field.ToString(newCtx);
			if (stringedField)
			{
				builder.Append(stringedField);
				if (i != fields.size - 1)
				{
					builder.Append(STRING_LIT(", "));
				}
			}
        }
        builder.Append(STRING_LIT(" }"));
	}
	return builder;
}

bool meTypeDescriptor::FromString(DeserializeContext& ctx) const
{
	// what we are deserializing from
	StringView str = StringView(ctx.inputData.data, ctx.inputData.size);

    // Handle null/empty string
    if (!str.data || str.len == 0 || !ShouldSerializeText()) 
    {
        return false;
    }
    
    // custom override
	if (strDeserializer)
	{
		return strDeserializer(*this, ctx);
	}
	ctx.parentType = this;

	// append multiple of the inner types for arrays
	if (thisType && TEST_BIT(flags, meTypeDescriptorFlag_ConstantArray))
	{
		//u32 numArrayElements = size / underlyingType->size;
		//for (u32 i = 0; i < numArrayElements; i++)
		//{
		//	meSpan arrayElement = str.OffsetView(underlyingType->size * i, underlyingType->size);
		//}
		UNIMPLEMENTED();
	}

    // If this is a primitive type with an underlying type, delegate to it
    if (thisType && fields.size == 0) 
    {
        return thisType->FromString(ctx);
    }

    // Handle primitive types based on name and size
    if (fields.size == 0) 
    {
        // Allocate memory for the primitive value
        meSpan result = ctx.outputData;
		ME_ASSERT(result);
        ME_MEMCLEAR(result, size);
        
        if (this == &TD_INT) 
        {
            s32 value = StringParseInt32(str);
            *((s32*)result.data) = value;
        }
        else if (this == &TD_UNSIGNED_INT) 
        {
            u32 value = StringParseUInt32(str);
            *((u32*)result.data) = value;
        }
        else if (this == &TD_LONGLONG) 
        {
            s64 value = StringParseInt64(str);
            *((s64*)result.data) = value;
        }
        else if (this == &TD_UNSIGNED_LONG_LONG) 
        {
            u64 value = StringParseUInt64(str);
            *((u64*)result.data) = value;
        }
        else if (this == &TD_SHORT) 
        {
            s16 value = (s16)StringParseInt32(str);
            *((s16*)result.data) = value;
        }
        else if (this == &TD_UNSIGNED_SHORT) 
        {
            u16 value = (u16)StringParseUInt32(str);
            *((u16*)result.data) = value;
        }
        else if (this == &TD_CHAR) 
        {
            s8 value = (s8)StringParseInt32(str);
            *((s8*)result.data) = value;
        }
        else if (this == &TD_UNSIGNED_CHAR) 
        {
            u8 value = (u8)StringParseUInt32(str);
            *((u8*)result.data) = value;
        }
        else if (this == &TD_FLOAT) 
        {
            float value = StringParseFloat(str);
            *((float*)result.data) = value;
        }
        else if (this == &TD_DOUBLE) 
        {
            double value = StringParseDouble(str);
            *((double*)result.data) = value;
        }
        else if (this == &TD_BOOL) 
        {
            bool value = false;
            if (StringCompare(str, STRING_LIT("true")) || StringCompare(str, STRING_LIT("True")) || 
                StringCompare(str, STRING_LIT("TRUE")) || StringCompare(str, STRING_LIT("1")))
            {
                value = true;
            }
            *((bool*)result.data) = value;
        }
        else if (this == &TD_VEC3)
        {
            glm::vec3 value = {};
            // Expecting format (x, y, z)
            str = EatChars(str, STRING_LIT("( "));

            StringView xStr = str;
            u32 offset = EatCharsOffset(xStr, ' ', true);
            xStr = xStr.OffsetView(0, offset);
            value.x = StringParseFloat(xStr);
            str = str.OffsetView(offset);
            str = EatChars(str, STRING_LIT(" "));

            StringView yStr = str;
            offset = EatCharsOffset(yStr, ' ', true);
            yStr = yStr.OffsetView(0, offset);
            value.y = StringParseFloat(yStr);
            str = str.OffsetView(offset);
            str = EatChars(str, STRING_LIT(" "));

            StringView zStr = str;
            offset = EatCharsOffset(zStr, ')');
            zStr = zStr.OffsetView(0, offset);
            value.z = StringParseFloat(zStr);
            str = EatChars(str, STRING_LIT(")"), true);
            str = EatChars(str, STRING_LIT("), "));

            *((glm::vec3*)result.data) = value;
        }
		else if (this == &TD_QUAT) // quat and vec4 are the same
		{
			glm::vec4 value = {};
			// Expecting format (x, y, z, w)
            str = EatChars(str, STRING_LIT("( "));

            StringView xStr = str;
            u32 offset = EatCharsOffset(xStr, ' ', true);
            xStr = xStr.OffsetView(0, offset);
            value.x = StringParseFloat(xStr);
            str = str.OffsetView(offset);
            str = EatChars(str, STRING_LIT(" "));

            StringView yStr = str;
            offset = EatCharsOffset(yStr, ' ', true);
            yStr = yStr.OffsetView(0, offset);
            value.y = StringParseFloat(yStr);
            str = str.OffsetView(offset);
            str = EatChars(str, STRING_LIT(" "));

			StringView zStr = str;
            offset = EatCharsOffset(yStr, ' ', true);
            zStr = zStr.OffsetView(0, offset);
            value.z = StringParseFloat(zStr);
            str = str.OffsetView(offset);
            str = EatChars(str, STRING_LIT(" "));

            StringView wStr = str;
            offset = EatCharsOffset(wStr, ')');
            wStr = wStr.OffsetView(0, offset);
            value.w = StringParseFloat(wStr);
            str = EatChars(str, STRING_LIT(")"), true);
            str = EatChars(str, STRING_LIT("), "));

            *((glm::vec4*)result.data) = value;
		}
        else 
        {
            UNIMPLEMENTED();
        }
        // changes to str are reflected back to the caller in this way
        // I.E. when we deserialize something, we "consume" it from the input data
        ctx.inputData = ctx.inputData.Subspan((u64)(str.data - ctx.inputData.data));
		return true;
    }
    else
	{
        str = EatChars(str, '{');
        str = EatChars(str, ' ');
		// Known number of elements in this input string
		for (u64 i = 0; i < fields.size; i++)
        {
            const meTypeDescriptor& field = fields[i];
            ME_ASSERT(field.offsetBits % 8 == 0);
            s32 len = EatCharsOffset(str, STRING_LIT(","), true);
            if (len == -1)
            {
                LOG_ERROR("Failed to find delimiter for field %.*s", STRING_VAARGS(field.name));
            }
            // Need to copy data from inputdata stringview into field outputdata
            DeserializeContext fieldCtx = ctx;
            fieldCtx.inputData = meSpan(str.data, str.len);
            fieldCtx.outputData = meSpan(ctx.outputData.data + (field.offsetBits / 8), field.size);
            if (!field.FromString(fieldCtx))
            {
				// TODO: differentiating errors from non-error situations would be good...
                continue;
            }
            str = StringView(fieldCtx.inputData);
            str = EatChars(str, STRING_LIT(", "));
        }
        str = EatChars(str, ' ');
        str = EatChars(str, '}');
	}
	return true;
}



StringView sizedBufferSerializer(
	const meTypeDescriptor& typedescriptor,
	SerializeContext ctx)
{
	meSpan fieldData = ctx.data;
	meAllocator* allocator = ctx.allocator;
	// the fielddata is just a pointer to a mespan, which ITSELF has the actual data
	meSpan dereferencedData = *(meSpan*)fieldData.data;
	Allocation mem = MEALLOC(allocator, dereferencedData.size);
	BufferCopy(mem, dereferencedData);
	return StringView(mem);
}

bool sizedBufferDeserializer(
	const meTypeDescriptor& typedescriptor,
	DeserializeContext& ctx)
{
	meSpan* outputSpan = (meSpan*)ctx.outputData.data;
	Allocation mem = MEALLOC(ctx.externalDataAllocator, ctx.inputData.size);
	BufferCopy(mem, ctx.inputData);
	ctx.outputDataExternal = mem;
	*outputSpan = mem;
	return true;
}

bool stringDeserializer(
	const meTypeDescriptor& typedescriptor,
	DeserializeContext& ctx)
{
	String* ownedStr = (String*)ctx.outputData.data;
	ownedStr->CopyOf(StringView::FromSpan(ctx.inputData), ctx.externalDataAllocator);
	ctx.outputDataExternal = meSpan(ownedStr->data, ownedStr->len);
	ctx.outputData = meSpan(ownedStr, sizeof(String));
	return true;
}

