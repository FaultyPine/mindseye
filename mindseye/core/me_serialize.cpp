#include "me_serialize.h"

#include "reflector/reflection_types.h"
#include "platform/me_os.h"
#include "core/me_math.h"
#include "core/containers/dynarray.h"

#include <external/json.hpp>
using json = nlohmann::json;

// =========================================================
// JSON Serialization Helpers
// =========================================================

// Forward declarations
static json JsonSerializeWithTypeDescriptor(
	const meTypeDescriptor& td, 
	void* data, 
	const meTypeDescriptor* parentType = nullptr);
static bool JsonDeserializeWithTypeDescriptor(
	const json& j, 
	const meTypeDescriptor& td, 
	void* outData, 
	meAllocator* allocator, 
	const meTypeDescriptor* parentType = nullptr);

// Convert a meTypeDescriptor + data pointer to a JSON value
static json JsonSerializeWithTypeDescriptor(
	const meTypeDescriptor& td, 
	void* data, 
	const meTypeDescriptor* parentType)
{
	if (!data || !td.ShouldSerializeText())
	{
		return json();
	}

	// Custom serializer override - use it and store as string
	if (td.strSerializer)
	{
		SerializeContext ctx = {};
		ctx.allocator = GetTLScratch();
		ctx.data = meSpan(data, td.size);
		ctx.parentType = parentType ? parentType : &td;
		StringView str = td.strSerializer(td, ctx);
		return std::string(str.data, str.len);
	}

	// Constant array
	if (td.thisType && TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray))
	{
		json arr = json::array();
		u32 numElements = td.size / td.thisType->size;
		for (u32 i = 0; i < numElements; i++)
		{
			void* elementData = (u8*)data + (td.thisType->size * i);
			arr.push_back(JsonSerializeWithTypeDescriptor(*td.thisType, elementData, &td));
		}
		return arr;
	}

	// Typedef/alias with underlying type but no fields
	if (td.thisType && td.fields.size == 0)
	{
		return JsonSerializeWithTypeDescriptor(*td.thisType, data, &td);
	}

	// Primitive types
	if (td.fields.size == 0)
	{
		if (&td == &TD_INT)                  return *((s32*)data);
		if (&td == &TD_UNSIGNED_INT)         return *((u32*)data);
		if (&td == &TD_LONGLONG)             return *((s64*)data);
		if (&td == &TD_UNSIGNED_LONG_LONG)   return *((u64*)data);
		if (&td == &TD_SHORT)                return *((s16*)data);
		if (&td == &TD_UNSIGNED_SHORT)       return *((u16*)data);
		if (&td == &TD_CHAR)                 return *((s8*)data);
		if (&td == &TD_UNSIGNED_CHAR)        return *((u8*)data);
		if (&td == &TD_FLOAT)                return *((float*)data);
		if (&td == &TD_DOUBLE)               return *((double*)data);
		if (&td == &TD_BOOL)                 return *((bool*)data);
		if (&td == &TD_VEC3)
		{
			float* v = (float*)data;
			return json::array({v[0], v[1], v[2]});
		}
		if (&td == &TD_QUAT)
		{
			float* v = (float*)data;
			return json::array({v[0], v[1], v[2], v[3]});
		}
		if (&td == &TD_STRINGVIEW)
		{
			StringView* sv = (StringView*)data;
			return std::string(sv->data, sv->len);
		}
		// Unknown primitive
		return json();
	}

	// Struct with fields
	json obj = json::object();
	for (u64 i = 0; i < td.fields.size; i++)
	{
		const meTypeDescriptor& field = td.fields[i];
		if (!field.ShouldSerializeText() || field.thisType == nullptr)
		{
			continue;
		}
		ME_ASSERT(field.offsetBits % 8 == 0);
		void* fieldData = (u8*)data + (field.offsetBits / 8);
		std::string fieldName(field.name.data, field.name.len);
		obj[fieldName] = JsonSerializeWithTypeDescriptor(field, fieldData, &td);
	}
	return obj;
}

// Deserialize JSON into a data buffer using meTypeDescriptor
static bool JsonDeserializeWithTypeDescriptor(const json& j, const meTypeDescriptor& td, void* outData, meAllocator* allocator, const meTypeDescriptor* parentType)
{
	if (j.is_null() || !td.ShouldSerializeText())
	{
		return false;
	}

	// Custom deserializer override
	if (td.strDeserializer)
	{
		std::string str;
		if (j.is_string())
		{
			str = j.get<std::string>();
		}
		else
		{
			str = j.dump();
		}
		DeserializeContext ctx = {};
		ctx.inputData = meSpan((char*)str.data(), str.size());
		ctx.outputData = meSpan(outData, td.size);
		ctx.externalDataAllocator = allocator;
		ctx.parentType = parentType ? parentType : &td;
		return td.strDeserializer(td, ctx);
	}

	// Constant array
	if (td.thisType && TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray))
	{
		if (!j.is_array()) return false;
		u32 numElements = td.size / td.thisType->size;
		u32 jsonSize = (u32)j.size();
		u32 count = (numElements < jsonSize) ? numElements : jsonSize;
		for (u32 i = 0; i < count; i++)
		{
			void* elementData = (u8*)outData + (td.thisType->size * i);
			JsonDeserializeWithTypeDescriptor(j[i], *td.thisType, elementData, allocator, &td);
		}
		return true;
	}

	// Typedef/alias with underlying type but no fields
	if (td.thisType && td.fields.size == 0)
	{
		return JsonDeserializeWithTypeDescriptor(j, *td.thisType, outData, allocator, &td);
	}

	// Primitive types
	if (td.fields.size == 0)
	{
		if (&td == &TD_INT)                  { *((s32*)outData) = j.get<s32>(); return true; }
		if (&td == &TD_UNSIGNED_INT)         { *((u32*)outData) = j.get<u32>(); return true; }
		if (&td == &TD_LONGLONG)             { *((s64*)outData) = j.get<s64>(); return true; }
		if (&td == &TD_UNSIGNED_LONG_LONG)   { *((u64*)outData) = j.get<u64>(); return true; }
		if (&td == &TD_SHORT)                { *((s16*)outData) = j.get<s16>(); return true; }
		if (&td == &TD_UNSIGNED_SHORT)       { *((u16*)outData) = j.get<u16>(); return true; }
		if (&td == &TD_CHAR)                 { *((s8*)outData) = j.get<s8>(); return true; }
		if (&td == &TD_UNSIGNED_CHAR)        { *((u8*)outData) = j.get<u8>(); return true; }
		if (&td == &TD_FLOAT)                { *((float*)outData) = j.get<float>(); return true; }
		if (&td == &TD_DOUBLE)               { *((double*)outData) = j.get<double>(); return true; }
		if (&td == &TD_BOOL)                 { *((bool*)outData) = j.get<bool>(); return true; }
		if (&td == &TD_VEC3)
		{
			if (!j.is_array() || j.size() < 3) return false;
			float* v = (float*)outData;
			v[0] = j[0].get<float>();
			v[1] = j[1].get<float>();
			v[2] = j[2].get<float>();
			return true;
		}
		if (&td == &TD_QUAT)
		{
			if (!j.is_array() || j.size() < 4) return false;
			float* v = (float*)outData;
			v[0] = j[0].get<float>();
			v[1] = j[1].get<float>();
			v[2] = j[2].get<float>();
			v[3] = j[3].get<float>();
			return true;
		}
		if (&td == &TD_STRINGVIEW)
		{
			// StringView deserialization needs external allocation
			std::string str = j.get<std::string>();
			Allocation mem = MEALLOC(allocator, str.size());
			memcpy(mem.data, str.data(), str.size());
			StringView* sv = (StringView*)outData;
			sv->data = (char*)mem.data;
			sv->len = (u32)str.size();
			return true;
		}
		return false;
	}

	// Struct with fields
	if (!j.is_object()) return false;
	for (u64 i = 0; i < td.fields.size; i++)
	{
		const meTypeDescriptor& field = td.fields[i];
		if (!field.ShouldSerializeText() || field.thisType == nullptr)
		{
			continue;
		}
		ME_ASSERT(field.offsetBits % 8 == 0);
		std::string fieldName(field.name.data, field.name.len);
		if (!j.contains(fieldName))
		{
			// Field not in JSON - leave as default
			continue;
		}
		void* fieldData = (u8*)outData + (field.offsetBits / 8);
		JsonDeserializeWithTypeDescriptor(j[fieldName], field, fieldData, allocator, &td);
	}
	return true;
}

// =========================================================
// Public API
// =========================================================

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
	json root = json::object();
	root["version"] = typeDesc.version;
	root["type"] = std::string(typeDesc.name.data, typeDesc.name.len);
	
	// Serialize all fields into the root object
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (!field.ShouldSerializeText() || field.thisType == nullptr)
		{
			continue;
		}
		ME_ASSERT(field.offsetBits % 8 == 0);
		void* fieldData = (u8*)data + (field.offsetBits / 8);
		std::string fieldName(field.name.data, field.name.len);
		// BOOKMARK: crash here on save
		root[fieldName] = JsonSerializeWithTypeDescriptor(field, fieldData, &typeDesc);
	}

	// Convert to string with pretty printing
	std::string jsonStr = {};
	try
	{
		jsonStr = root.dump(4);
	}
	catch (const json::type_error& e)
	{
		LOG_ERROR("JSON dump error: %s", e.what());
		return meSerializeResult::SER_FAILURE;
	}
	
	// Allocate and copy to output
	Allocation mem = MEALLOC(allocator, jsonStr.size() + 1);
	memcpy(mem.data, jsonStr.data(), jsonStr.size());
	((char*)mem.data)[jsonStr.size()] = '\0';
	outResult = StringView((const char*)mem.data, (u32)jsonStr.size());
	
	return meSerializeResult::SER_SUCCESS;
}

meSerializeResult DeserializeFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	meSpan outBuffer)
{
	// Parse JSON
	json root;
	try
	{
		root = json::parse(inText.data, inText.data + inText.len);
	}
	catch (const json::parse_error& e)
	{
		LOG_ERROR("JSON parse error: %s", e.what());
		return meSerializeResult::SER_FAILURE;
	}
	// Check type
	if (root.contains("type"))
	{
		std::string typeStr = root["type"].get<std::string>();
		StringView expectedType = typeDesc.name;
		if (typeStr != std::string(expectedType.data, expectedType.len))
		{
			LOG_ERROR("Type mismatch deserializing from JSON. Expected %.*s but got %s", 
				STRING_VAARGS(typeDesc.name), 
				typeStr.c_str());
			return meSerializeResult::SER_FAILURE;
		}
	}

	// Check version
	if (root.contains("version"))
	{
		s32 version = root["version"].get<s32>();
		if (version != typeDesc.version)
		{
			LOG_ERROR("Version mismatch deserializing from JSON for type %.*s. Expected version %d but got version %d", 
				STRING_VAARGS(typeDesc.name), 
				typeDesc.version,
				version);
			return meSerializeResult::SER_VERSION_MISMATCH;
		}
	}

	// Deserialize fields
	ME_ASSERT(outBuffer.size == typeDesc.size);
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (!field.ShouldSerializeText() || field.thisType == nullptr)
		{
			continue;
		}
		ME_ASSERT(field.offsetBits % 8 == 0);
		std::string fieldName(field.name.data, field.name.len);
		if (!root.contains(fieldName))
		{
			// Field not in JSON - leave as default
			continue;
		}
		void* fieldData = (u8*)outBuffer.data + (field.offsetBits / 8);
		JsonDeserializeWithTypeDescriptor(root[fieldName], field, fieldData, allocator, &typeDesc);
	}

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
