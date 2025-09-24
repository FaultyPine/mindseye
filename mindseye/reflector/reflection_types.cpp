
#include "reflection_types.h"
#include "core/me_string.h"

meTypeDescriptor TD_UNSIGNED_INT = { .name = STRING_LIT("unsigned int"), .size = 4, .align = 4 };
meTypeDescriptor TD_INT = { .name = STRING_LIT("int"), .size = 4, .align = 4 };
meTypeDescriptor TD_UNSIGNED_SHORT = { .name = STRING_LIT("unsigned short"), .size = 2, .align = 2 };
meTypeDescriptor TD_SHORT = { .name = STRING_LIT("short"), .size = 2, .align = 2 };
meTypeDescriptor TD_UNSIGNED_LONG = { .name = STRING_LIT("unsigned long"), .size = 8, .align = 8 };
meTypeDescriptor TD_LONG = { .name = STRING_LIT("long"), .size = 8, .align = 8 };
meTypeDescriptor TD_LONGLONG = { .name = STRING_LIT("long long"), .size = 8, .align = 8 };
meTypeDescriptor TD_UNSIGNED_LONGLONG = { .name = STRING_LIT("unsigned long long"), .size = 8, .align = 8 };
meTypeDescriptor TD_FLOAT = { .name = STRING_LIT("float"), .size = 4, .align = 4 };
meTypeDescriptor TD_DOUBLE = { .name = STRING_LIT("double"), .size = 8, .align = 8 };
meTypeDescriptor TD_BOOL = { .name = STRING_LIT("bool"), .size = 1, .align = 1 };
meTypeDescriptor TD_CHAR = { .name = STRING_LIT("char"), .size = 1, .align = 1 };
meTypeDescriptor TD_UNSIGNED_CHAR = {.name = STRING_LIT("unsigned char"), .size = 1, .align = 1 };
meTypeDescriptor TD_WCHAR = { .name = STRING_LIT("wchar_t"), .size = 4, .align = 4 };
meTypeDescriptor TD_POINTER = { .name = STRING_LIT("meSerializedPtr"), .size = 8, .align = 8 };


// TODO: THIS IS VERY UNTESTED
StringView meTypeDescriptor::ToString(meAllocator* allocator, meSpan data) const
{
    // Allocate a reasonable buffer for the string representation
    const u32 BUFFER_SIZE = 4096;
    Allocation memory = MEALLOC(allocator, BUFFER_SIZE);
    ME_MEMCLEAR(memory, BUFFER_SIZE);
    meSpan buffer = memory;
    u32 offset = 0;
    
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
    
    // Handle primitive types based on name and size
    if (fields.size == 0) 
	{
        if (this == &TD_INT) 
		{
            StringFormatIntoBuf(buffer, "%d", *((s32*)data.data));
        }
        else if (this == &TD_UNSIGNED_INT) 
		{
            StringFormatIntoBuf(buffer, "%u", *((u32*)data.data));
        }
        else if (this == &TD_LONGLONG) 
		{
            StringFormatIntoBuf(buffer, "%lld", *((s64*)data.data));
        }
        else if (this == &TD_UNSIGNED_LONGLONG) 
		{
            StringFormatIntoBuf(buffer, "%llu", *((u64*)data.data));
        }
        else if (this == &TD_SHORT) 
		{
            StringFormatIntoBuf(buffer, "%d", (s32)*((s16*)data.data));
        }
        else if (this == &TD_UNSIGNED_SHORT) 
		{
            StringFormatIntoBuf(buffer, "%u", (u32)*((u16*)data.data));
        }
        else if (this == &TD_CHAR) 
		{
            StringFormatIntoBuf(buffer, "%d", (s32)*((s8*)data.data));
        }
        else if (this == &TD_UNSIGNED_CHAR) 
		{
            StringFormatIntoBuf(buffer, "%u", (u32)*((u8*)data.data));
        }
        else if (this == &TD_FLOAT) 
		{
            StringFormatIntoBuf(buffer, "%.6f", *((float*)data.data));
        }
        else if (this == &TD_DOUBLE) 
		{
            StringFormatIntoBuf(buffer, "%.15f", *((double*)data.data));
        }
        else if (this == &TD_BOOL) 
		{
            StringFormatIntoBuf(buffer, "%s", *((bool*)data.data) ? "true" : "false");
        }
        else 
		{
			UNIMPLEMENTED();
        }
    }
    else 
	{
        // This is a composite type with fields
        StringFormatIntoBuf(buffer, "%s { ", name.data ? name.data : "struct");
        offset = CStringLength(buffer);
        
        for (u64 i = 0; i < fields.size && offset < BUFFER_SIZE - 100; i++) 
		{
            const meTypeDescriptor& field = fields[i];
            
            u32 fieldOffset = field.offsetBits / 8;
            
            if (fieldOffset + field.size > data.size) 
			{
                continue;
            }
            
            meSpan fieldData;
            fieldData.data = ((u8*)data.data) + fieldOffset;
            fieldData.size = field.size;
            
            // Recursively convert field to string
            StringView fieldStr = field.ToString(allocator, fieldData);
            
            // Add field name and value
            offset += StringFormatIntoBuf(buffer.Subspan(offset), "%s: %.*s", 
								 field.name.data ? field.name.data : "field", 
								 (int)fieldStr.len, fieldStr.data);
            
            // Add comma separator if not the last field
            if (i < fields.size - 1) 
			{
                offset += StringFormatIntoBuf(buffer.Subspan(offset), ", ");
            }
        }
        StringFormatIntoBuf(buffer.Subspan(offset), " }");
    }
    return StringView((const char*)memory, CStringLength((const char*)memory));
}

// TODO: THIS IS VERY UNTESTED
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
        else if (this == &TD_UNSIGNED_LONGLONG) 
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
        
        return result;
    }
    else 
    {
        // This is a composite type with fields
        // Expected format: "TypeName { field1: value1, field2: value2, ... }"
        
        // Allocate memory for the struct
        Allocation memory = MEALLOC(allocator, size);
        ME_MEMCLEAR(memory, size);
        meSpan result = memory;
        
        // Find the opening brace
        char* braceStart = nullptr;
        for (u64 i = 0; i < str.len; i++) 
        {
            if (str.data[i] == '{') 
            {
                braceStart = &str.data[i + 1];
                break;
            }
        }
        
        if (!braceStart) 
        {
            return result; // Invalid format, return zeroed struct
        }
        
        // Find the closing brace
        char* braceEnd = nullptr;
        for (s64 i = str.len - 1; i >= 0; i--) 
        {
            if (str.data[i] == '}') 
            {
                braceEnd = &str.data[i];
                break;
            }
        }
        
        if (!braceEnd || braceEnd <= braceStart) 
        {
            return result; // Invalid format, return zeroed struct
        }
        
        // Extract the content between braces
        StringView content;
        content.data = braceStart;
        content.len = braceEnd - braceStart;
        
        // Parse field assignments
        char* current = content.data;
        char* end = content.data + content.len;
        
        while (current < end) 
        {
            // Skip whitespace
            while (current < end && (*current == ' ' || *current == '\t' || *current == '\n')) 
            {
                current++;
            }
            
            if (current >= end) break;
            
            // Find field name (everything before ':')
            char* fieldNameStart = current;
            while (current < end && *current != ':') 
            {
                current++;
            }
            
            if (current >= end) break;
            
            StringView fieldName;
            fieldName.data = fieldNameStart;
            fieldName.len = current - fieldNameStart;
            
            // Trim whitespace from field name
            while (fieldName.len > 0 && (fieldName.data[fieldName.len-1] == ' ' || fieldName.data[fieldName.len-1] == '\t')) 
            {
                fieldName.len--;
            }
            
            current++; // Skip ':'
            
            // Skip whitespace after ':'
            while (current < end && (*current == ' ' || *current == '\t')) 
            {
                current++;
            }
            
            // Find field value (everything before ',' or end)
            char* fieldValueStart = current;
            s32 braceDepth = 0;
            while (current < end) 
            {
                if (*current == '{') braceDepth++;
                else if (*current == '}') braceDepth--;
                else if (*current == ',' && braceDepth == 0) break;
                current++;
            }
            
            StringView fieldValue;
            fieldValue.data = fieldValueStart;
            fieldValue.len = current - fieldValueStart;
            
            // Trim whitespace from field value
            while (fieldValue.len > 0 && (fieldValue.data[fieldValue.len-1] == ' ' || fieldValue.data[fieldValue.len-1] == '\t')) 
            {
                fieldValue.len--;
            }
            
            // Find the matching field in our type descriptor
            for (u64 i = 0; i < fields.size; i++) 
            {
                const meTypeDescriptor& field = fields[i];
                if (field.name.data && StringCompare(fieldName, StringView(field.name.data, CStringLength(field.name.data)))) 
                {
                    // Parse the field value recursively
                    meSpan fieldData = field.FromString(allocator, fieldValue);
                    
                    if (fieldData.data && fieldData.size > 0) 
                    {
                        // Copy the parsed data into the correct offset in our result
                        u32 fieldOffset = field.offsetBits / 8;
                        if (fieldOffset + field.size <= size) 
                        {
                            u32 copySize = fieldData.size < field.size ? fieldData.size : field.size;
                            ME_MEMCPY(((u8*)result.data) + fieldOffset, fieldData.data, copySize);
                        }
                    }
                    break;
                }
            }
            
            // Skip ',' if present
            if (current < end && *current == ',') 
            {
                current++;
            }
        }
        
        return result;
    }
}