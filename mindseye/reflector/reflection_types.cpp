
#include "reflection_types.h"
#include "core/me_string.h"

meTypeDescriptor TD_UNSIGNED_INT = { .name = STRING_LIT("unsigned int"), .size = 4, .align = 4 };
meTypeDescriptor TD_INT = { .name = STRING_LIT("int"), .size = 4, .align = 4 };
meTypeDescriptor TD_UNSIGNED_SHORT = { .name = STRING_LIT("unsigned short"), .size = 2, .align = 2 };
meTypeDescriptor TD_SHORT = { .name = STRING_LIT("short"), .size = 2, .align = 2 };
meTypeDescriptor TD_UNSIGNED_LONG = { .name = STRING_LIT("unsigned long"), .size = 8, .align = 8 };
meTypeDescriptor TD_LONG = { .name = STRING_LIT("long"), .size = 8, .align = 8 };
meTypeDescriptor TD_LONGLONG = { .name = STRING_LIT("long long"), .size = 8, .align = 8 };
meTypeDescriptor TD_UNSIGNED_LONG_LONG = { .name = STRING_LIT("unsigned long long"), .size = 8, .align = 8 };
meTypeDescriptor TD_FLOAT = { .name = STRING_LIT("float"), .size = 4, .align = 4 };
meTypeDescriptor TD_DOUBLE = { .name = STRING_LIT("double"), .size = 8, .align = 8 };
meTypeDescriptor TD_BOOL = { .name = STRING_LIT("bool"), .size = 1, .align = 1 };
meTypeDescriptor TD_CHAR = { .name = STRING_LIT("char"), .size = 1, .align = 1 };
meTypeDescriptor TD_UNSIGNED_CHAR = {.name = STRING_LIT("unsigned char"), .size = 1, .align = 1 };
meTypeDescriptor TD_WCHAR = { .name = STRING_LIT("wchar_t"), .size = 4, .align = 4 };

StringView sizedBufferSerializer(meAllocator*, meSpan);
meSpan sizedBufferDeserializer(meAllocator*, StringView);
meSpan stringDeserializer(meAllocator* allocator, StringView str);

// arbitrary spans, when serialized to string will just have the binary written out as is, which is why the string serializer works for both
meTypeDescriptor TD_SPAN = { .name = STRING_LIT("span"), .size = sizeof(meSpan), .align = alignof(meSpan), .strSerializer = sizedBufferSerializer, .strDeserializer = sizedBufferDeserializer };
meTypeDescriptor TD_STRINGVIEW = { .name = STRING_LIT("StringView"), .size = sizeof(StringView), .align = alignof(StringView), .strSerializer = sizedBufferSerializer, .strDeserializer = sizedBufferDeserializer };
// NOTE: We can reuse the sizedbufferserializer ONLY because String follows the same pattern as StringView and meSpan
// where the first param is a data pointer and the second is the 64bit size.
meTypeDescriptor TD_STRING = { .name = STRING_LIT("String"), .size = sizeof(String), .align = alignof(String), .strSerializer = sizedBufferSerializer, .strDeserializer = stringDeserializer };

StringView meTypeDescriptor::ToString(meAllocator* allocator, meSpan data) const
{
    // Allocate a reasonable buffer for the string representation
	StringBuilder builder = StringBuilder(allocator);
    
    // Handle null/empty data
    if (!data.data || data.size == 0) 
	{
		return {};
    }
    
    // If this is a primitive type with an underlying type, delegate to it
    if (underlyingType != nullptr && fields.size == 0) 
	{
        return underlyingType->ToString(allocator, data);
    }

    // custom override
	if (strSerializer)
	{
		return strSerializer(allocator, data);
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
        else 
		{
			UNIMPLEMENTED();
        }
    }
	else
	{
		// composite struct types not supported
		UNIMPLEMENTED();
	}
	return builder;
}

meSpan meTypeDescriptor::FromString(meAllocator* allocator, StringView str) const
{
    // Handle null/empty string
    if (!str.data || str.len == 0) 
    {
        return {};
    }
    
    // If this is a primitive type with an underlying type, delegate to it
    if (underlyingType != nullptr && fields.size == 0) 
    {
        return underlyingType->FromString(allocator, str);
    }

    // custom override
	if (strDeserializer)
	{
		return strDeserializer(allocator, str);
	}

    // Handle primitive types based on name and size
    if (fields.size == 0) 
    {
        // Allocate memory for the primitive value
        Allocation memory = MEALLOC(allocator, size);
        ME_MEMCLEAR(memory, size);
        meSpan result = memory;
        
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
            // Case-insensitive comparison for boolean values
            if (StringCompare(str, STRING_LIT("true")) || StringCompare(str, STRING_LIT("True")) || 
                StringCompare(str, STRING_LIT("TRUE")) || StringCompare(str, STRING_LIT("1")))
            {
                value = true;
            }
            *((bool*)result.data) = value;
        }
		else if (this == &TD_STRINGVIEW)
		{
			bool success = StringCopy(StringView((const char*)result.data, result.size), str);
			ME_ASSERT(success);
		}
        else 
        {
            UNIMPLEMENTED();
        }
        return result;
    }
    else
	{
		// composite struct types not supported
		UNIMPLEMENTED();
	}
	return {};
}



StringView sizedBufferSerializer(meAllocator* allocator, meSpan fieldData)
{
	// the fielddata is just a pointer to a mespan, which ITSELF has the actual data
	meSpan dereferencedData = *(meSpan*)fieldData.data;
	Allocation mem = MEALLOC(allocator, dereferencedData.size);
	StringCopy(StringView(mem), StringView(dereferencedData));
	return StringView(mem);
}

meSpan sizedBufferDeserializer(meAllocator* allocator, StringView str)
{
	Allocation mem = MEALLOC(allocator, str.len);
	StringCopy(StringView(mem), StringView(str));
	return mem;
}

meSpan stringDeserializer(meAllocator* allocator, StringView str)
{
	String* ownedStr = MENEW(allocator, String);
	ownedStr->CopyOf(str, allocator);
	return meSpan(ownedStr, sizeof(String));
}

