
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
bool sizedBufferDeserializer(DeserializeContext& ctx);
bool stringDeserializer(DeserializeContext& ctx);

// NOTE: We can reuse the sizedbufferserializer ONLY because meSpan, StringView, and String follow a similar pattern internally
// where the first param is a data pointer and the second is the 64bit size.
meTypeDescriptor TD_SPAN = { .name = STRING_LIT("span"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(meSpan), .align = alignof(meSpan), .strSerializer = sizedBufferSerializer, .strDeserializer = sizedBufferDeserializer };
meTypeDescriptor TD_STRINGVIEW = { .name = STRING_LIT("StringView"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(StringView), .align = alignof(StringView), .strSerializer = sizedBufferSerializer, .strDeserializer = sizedBufferDeserializer };
meTypeDescriptor TD_STRING = { .name = STRING_LIT("String"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(String), .align = alignof(String), .strSerializer = sizedBufferSerializer, .strDeserializer = stringDeserializer };

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
		// TODO: composite struct types not supported
		UNIMPLEMENTED();
	}
	return builder;
}

bool meTypeDescriptor::FromString(DeserializeContext& ctx) const
{
	// what we are deserializing from
	StringView str = StringView(ctx.inputData.data, ctx.inputData.size);

    // Handle null/empty string
    if (!str.data || str.len == 0) 
    {
        return false;
    }
    
    // If this is a primitive type with an underlying type, delegate to it
    if (underlyingType != nullptr && fields.size == 0) 
    {
        return underlyingType->FromString(ctx);
    }

    // custom override
	if (strDeserializer)
	{
		return strDeserializer(ctx);
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
            // Case-insensitive comparison for boolean values
            if (StringCompare(str, STRING_LIT("true")) || StringCompare(str, STRING_LIT("True")) || 
                StringCompare(str, STRING_LIT("TRUE")) || StringCompare(str, STRING_LIT("1")))
            {
                value = true;
            }
            *((bool*)result.data) = value;
        }
        else 
        {
            UNIMPLEMENTED();
        }
		return true;
    }
    else
	{
		// TODO: composite struct types not supported
		UNIMPLEMENTED();
	}
	return false;
}



StringView sizedBufferSerializer(meAllocator* allocator, meSpan fieldData)
{
	// the fielddata is just a pointer to a mespan, which ITSELF has the actual data
	meSpan dereferencedData = *(meSpan*)fieldData.data;
	Allocation mem = MEALLOC(allocator, dereferencedData.size);
	BufferCopy(mem, dereferencedData);
	return StringView(mem);
}

bool sizedBufferDeserializer(DeserializeContext& ctx)
{
	meSpan* outputSpan = (meSpan*)ctx.outputData.data;
	Allocation mem = MEALLOC(ctx.externalDataAllocator, ctx.inputData.size);
	BufferCopy(mem, ctx.inputData);
	ctx.outputDataExternal = mem;
	*outputSpan = mem;
	return true;
}

bool stringDeserializer(DeserializeContext& ctx)
{
	String* ownedStr = (String*)ctx.outputData.data;
	ownedStr->CopyOf(StringView::FromSpan(ctx.inputData), ctx.externalDataAllocator);
	ctx.outputDataExternal = meSpan(ownedStr->data, ownedStr->len);
	ctx.outputData = meSpan(ownedStr, sizeof(String));
	return true;
}

