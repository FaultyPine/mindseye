#include "me_serialize.h"

#include "reflector/reflection_types.h"
#include "platform/me_os.h"
#include "core/me_math.h"
#include "core/containers/dynarray.h"
#include "asset/me_asset.h"

#include <external/json.hpp>
using json = nlohmann::json;

void sizedBufferSerializer(
	const meTypeDescriptor& typedescriptor,
	SerializeContext& ctx)
{
	meSpan fieldData = ctx.data;
	meSpan dereferencedData = *(meSpan*)fieldData.data;
	json& out = *(json*)ctx.outputData.data;
	out = std::string(dereferencedData.data, dereferencedData.size);
}

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
	if (td.serializerFn)
	{
		json result;
		SerializeContext ctx = {};
		ctx.allocator = GetTLScratch();
		ctx.data = meSpan(data, td.size);
		ctx.outputData = meSpan(&result, sizeof(json));
		ctx.parentType = parentType ? parentType : &td;
		td.serializerFn(td, ctx);
		return result;
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
static bool JsonDeserializeWithTypeDescriptor(
	const json& j, 
	const meTypeDescriptor& td, 
	void* outData, 
	meAllocator* allocator, 
	const meTypeDescriptor* parentType)
{
	if (j.is_null() || !td.ShouldSerializeText())
	{
		return false;
	}

	// Custom deserializer override
	if (td.deserializerFn)
	{
		std::string str;
		if (j.is_string())
		{
			str = j.get<std::string>();
		}
		else
		{
			str = j.dump(4);
		}
		DeserializeContext ctx = {};
		ctx.inputData = meSpan((char*)str.data(), str.size());
		ctx.outputData = meSpan(outData, td.size);
		ctx.externalDataAllocator = allocator;
		ctx.parentType = parentType ? parentType : &td;
		return td.deserializerFn(td, ctx);
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
	if (!j.is_object())
	{
		LOG_WARN("Tried to deserialize an object but the parsed json isn't an object?");
		return false;
	}
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


bool meFieldsEqual(
	const meTypeDescriptor& td,
	const void* a,
	const void* b,
	const meTypeDescriptor* parentType)
{
	if (!td.ShouldSerializeText())
	{
		return true;
	}

	if (td.thisType && TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray))
	{
		u32 elemSize = td.thisType->size;
		if (elemSize == 0) return memcmp(a, b, td.size) == 0;
		u32 numElements = td.size / elemSize;
		for (u32 i = 0; i < numElements; i++)
		{
			const void* aElem = (const u8*)a + (elemSize * i);
			const void* bElem = (const u8*)b + (elemSize * i);
			if (!meFieldsEqual(*td.thisType, aElem, bElem, &td)) return false;
		}
		return true;
	}

	if (td.thisType)
	{
		return meFieldsEqual(*td.thisType, a, b, &td);
	}

	if (td.equalsFn)
	{
		return td.equalsFn(td, a, b);
	}

	if (td.fields.size > 0)
	{
		for (u64 i = 0; i < td.fields.size; i++)
		{
			const meTypeDescriptor& field = td.fields[i];
			if (!field.ShouldSerializeText() || field.thisType == nullptr) continue;
			ME_ASSERT(field.offsetBits % 8 == 0);
			const void* aField = (const u8*)a + (field.offsetBits / 8);
			const void* bField = (const u8*)b + (field.offsetBits / 8);
			if (!meFieldsEqual(field, aField, bField, &td)) return false;
		}
		return true;
	}

    // icky semi-last-resort. Compare the result of serializing both objects
	// if (td.serializerFn)
	// {
	// 	json ja, jb;
	// 	SerializeContext ctxA = {};
	// 	ctxA.allocator = GetTLScratch();
	// 	ctxA.data = meSpan((void*)a, td.size);
	// 	ctxA.outputData = meSpan(&ja, sizeof(json));
	// 	ctxA.parentType = parentType ? parentType : &td;
	// 	td.serializerFn(td, ctxA);

	// 	SerializeContext ctxB = {};
	// 	ctxB.allocator = GetTLScratch();
	// 	ctxB.data = meSpan((void*)b, td.size);
	// 	ctxB.outputData = meSpan(&jb, sizeof(json));
	// 	ctxB.parentType = parentType ? parentType : &td;
	// 	td.serializerFn(td, ctxB);
	// 	return ja == jb;
	// }

	// Last resort
	return memcmp(a, b, td.size) == 0;
}

// Override serialization
// only includes fields whose value differs between instanceData and
// templateData. The asset header (MAID field named "header") is always
// written so the deserializer can locate the template.
meSerializeResult SerializeOverridesToTextBlocking(
	const meTypeDescriptor& typeDesc,
	void* instanceData,
	void* templateData,
	meAllocator* allocator,
	StringView& outResult)
{
	json root = json::object();
	root["version"] = typeDesc.version;
	root["type"] = std::string(typeDesc.name.data, typeDesc.name.len);

	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (!field.ShouldSerializeText() || field.thisType == nullptr)
		{
			continue;
		}
		ME_ASSERT(field.offsetBits % 8 == 0);
		void* aField = (u8*)instanceData + (field.offsetBits / 8);
		void* bField = (u8*)templateData + (field.offsetBits / 8);

		bool isHeader = (field.thisType == &TD_MAID)
			&& StringCompare(field.name, STRING_LIT(ME_ASSET_HEADER_FIELDNAME));
		if (!isHeader && meFieldsEqual(field, aField, bField, &typeDesc))
		{
			continue;
		}

		std::string fieldName(field.name.data, field.name.len);
		root[fieldName] = JsonSerializeWithTypeDescriptor(field, aField, &typeDesc);
	}

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

	Allocation mem = MEALLOC(allocator, jsonStr.size() + 1);
	ME_MEMCPY(mem.data, jsonStr.data(), jsonStr.size());
	((char*)mem.data)[jsonStr.size()] = '\0';
	outResult = StringView((const char*)mem.data, (u32)jsonStr.size());

	return meSerializeResult::SER_SUCCESS;
}

meSerializeResult DeserializeOverridesFromTextBlocking(
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	StringView inText,
	const void* templateData,
	meSpan outBuffer)
{
	ME_ASSERT(outBuffer.size == typeDesc.size);
	ME_ASSERT(templateData);

	// Seed outBuffer with the template payload.
	// external-pointer fields (strings, spans, dynarrays) still alias the
	// template's memory until an override below rewrites them. For POD fields
	// that aren't overridden, this is exactly the behavior we want.
	ME_MEMCPY(outBuffer.data, templateData, typeDesc.size);

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

	if (root.contains("type"))
	{
		std::string typeStr = root["type"].get<std::string>();
		StringView expectedType = typeDesc.name;
		if (typeStr != std::string(expectedType.data, expectedType.len))
		{
			LOG_ERROR("Type mismatch deserializing overrides. Expected %.*s but got %s",
				STRING_VAARGS(typeDesc.name), typeStr.c_str());
			return meSerializeResult::SER_FAILURE;
		}
	}
	if (root.contains("version"))
	{
		s32 version = root["version"].get<s32>();
		if (version != typeDesc.version)
		{
			LOG_ERROR("Version mismatch deserializing overrides for %.*s: expected %d got %d",
				STRING_VAARGS(typeDesc.name), typeDesc.version, version);
			return meSerializeResult::SER_VERSION_MISMATCH;
		}
	}

	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (!field.ShouldSerializeText() || field.thisType == nullptr) continue;
		ME_ASSERT(field.offsetBits % 8 == 0);
		std::string fieldName(field.name.data, field.name.len);
		if (!root.contains(fieldName))
		{
			// Not overridden - template's value already lives in outBuffer.
			continue;
		}
		void* fieldData = (u8*)outBuffer.data + (field.offsetBits / 8);
		JsonDeserializeWithTypeDescriptor(root[fieldName], field, fieldData, allocator, &typeDesc);
	}

	return meSerializeResult::SER_SUCCESS;
}


meSerializeResult DeserializeFromFileBlocking(
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
	ME_MEMCPY(mem.data, jsonStr.data(), jsonStr.size());
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



void DynArraySerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext& ctx)
{
	const meTypeDescriptor* parentType = ctx.parentType;
	// DynArray is templated, and so requires the parent type to understand the template args, see comment in meTypeDescriptor struct
	ME_ASSERT(parentType);
	meSpanTyped<meTypeDescriptor*> templatedTypes = parentType->templatedTypes;
	ME_ASSERT(templatedTypes);
	ME_ASSERT(templatedTypes.size == 1);
	const meTypeDescriptor& templateArg = *templatedTypes[0];
	const meSpan& dynArrayData = ctx.data;
	DynArrayAny& arr = *(DynArrayAny*)dynArrayData.data;
	u32 size = DynArrayGetSize(arr);
	u32 stride = DynArrayGetStride(arr);
	json j = json::array();
	for (u32 i = 0; i < size; i++)
	{
		meSpan elementSpan = meSpan(&arr[i * stride], templateArg.size);
		SerializeContext elementCtx = ctx;
		elementCtx.data = elementSpan;
		json serializedElement = JsonSerializeWithTypeDescriptor(templateArg, elementSpan.data, ctx.parentType);
		j.push_back(serializedElement);
	}
	json& out = *(json*)ctx.outputData.data;
	out = std::move(j);
}

bool DynArrayDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx)
{
	const meTypeDescriptor* parentType = ctx.parentType;
	// DynArray is templated, and so requires the parent type to understand the template args, see comment in meTypeDescriptor struct
	ME_ASSERT(parentType);
	meSpanTyped<meTypeDescriptor*> templatedTypes = parentType->templatedTypes;
	ME_ASSERT(templatedTypes);
	ME_ASSERT(templatedTypes.size == 1);
	const meTypeDescriptor& templateArg = *templatedTypes[0];

	StringView str = StringView(ctx.inputData.data, ctx.inputData.size);
	json root;
	try
	{
		root = json::parse(str.data, str.data + str.len);
	}
	catch (const json::parse_error& e)
	{
		LOG_ERROR("JSON dynarray parse error: %s", e.what());
		return false;
	}
	bool result = false;
	DynArrayAny* array = (DynArrayAny*)ctx.outputData.data;
	// a byte array which is our "type erasure". Later becomes the actual typed array in the deserialized struct
	*array = DynArrayCreate<u8>(ctx.externalDataAllocator, DynArrayDefaultCapacity, templateArg.size);
	for (auto& element : root)
	{
		Allocation elementData = MEALLOC(ctx.externalDataAllocator, templateArg.size);
		result &= JsonDeserializeWithTypeDescriptor(element, templateArg, elementData, ctx.externalDataAllocator, parentType);
		templateArg.setToDefaultsFn(elementData);
        DynArrayPush(*array, (u8*)elementData, elementData.size);
	}
	return result;
}

bool DynArrayEqualsFn(
    const meTypeDescriptor& td,
    const void* a,
    const void* b)
{
    // BOOKMARK:
    UNIMPLEMENTED();
    return true;
}


void meAssetSerializerToStringFn(const meTypeDescriptor& td, SerializeContext& ctx) {
    meAsset* asset = (meAsset*)ctx.data.data;
    json& out = *(json*)ctx.outputData.data;

    bool isInstance = asset->runtimeHandle
                   && !asset->runtimeHandle.IsTemplateAsset()
                   && asset->loadStage == Loaded
                   && asset->id; // has a backing template
    if (!isInstance)
    {
        out = json::object();
        out["id"]   = (u64)asset->id.GetID();
        out["type"] = (u32)asset->id.GetType();
        return;
    }

    meAssetLoader* loader = meAssetSystemGet().assetLoaders[asset->id.GetType()];
    ME_ASSERT(loader);

    MAID templateMaid = asset->id;
    meAssetRequestLoad(&templateMaid, 1);
    meAssetWaitUntilLoadstage({ &templateMaid, 1 }, Loaded);
    meAsset* tmpl = meAssetTryGet(templateMaid);
    ME_ASSERT(tmpl && tmpl->runtimeHandle.IsTemplateAsset());

    void* templateData = loader->resourcePool->GetOpaque(tmpl->runtimeHandle);
    void* instanceData = loader->resourcePool->GetOpaque(asset->runtimeHandle);

    StringView delta = {};
    SerializeOverridesToTextBlocking(
        *loader->assetTypeDesc, instanceData, templateData,
        ctx.allocator, delta);
    // embed the delta document under this meAsset field
    out = json::parse(delta.data, delta.data + delta.len);
}

bool meAssetDeserializerFromStringFn(const meTypeDescriptor& td, DeserializeContext& ctx)
{
    meAsset* outAsset = (meAsset*)ctx.outputData.data;
    *outAsset = meAsset();

    StringView inText = StringView(ctx.inputData.data, ctx.inputData.size);
    json root;
    try
    {
        root = json::parse(inText.data, inText.data + inText.len);
    }
    catch (const json::parse_error& e)
    {
        LOG_ERROR("meAsset JSON parse error: %s", e.what());
        return false;
    }

    if (!root.is_object())
    {
        LOG_ERROR("meAsset JSON must be an object");
        return false;
    }

    // Distinguish "flat template ref" ({ id, type:<number> }) from
    // "override document" ({ version, type:<string>, header:{...}, ... }).
    // The override doc has a string-valued "type" (the type name) and a
    // "header" field containing the MAID.
    bool isOverrideDoc = root.contains("header")
        || (root.contains("type") && root["type"].is_string());

    if (!isOverrideDoc)
    {
        // Template ref: just populate the MAID. 
        if (!root.contains("id") || !root.contains("type"))
        {
            return false;
        }
        u64 id = root["id"].get<u64>();
        u32 typeInt = root["type"].get<u32>();
        outAsset->id = MAID(id, (meAssetType)typeInt);
        outAsset->runtimeHandle = EYE_INVALID;
        outAsset->loadStage = Unloaded;
        return true;
    }

    // Override document: we need the template loaded, then allocate an
    // instance asset and populate it
    if (!root.contains("header"))
    {
        LOG_ERROR("meAsset override doc missing 'header' field");
        return false;
    }
    const json& headerJson = root["header"];
    if (!headerJson.is_object() || !headerJson.contains("id") || !headerJson.contains("type"))
    {
        LOG_ERROR("meAsset override header malformed");
        return false;
    }
    u64 headerId = headerJson["id"].get<u64>();
    u32 headerType = headerJson["type"].get<u32>();
    MAID templateMaid = MAID(headerId, (meAssetType)headerType);

    meAssetSystem& assetSystem = meAssetSystemGet();
    meAssetLoader* loader = assetSystem.assetLoaders[templateMaid.GetType()];
    if (!loader)
    {
        LOG_ERROR("No asset loader for type %u", (u32)templateMaid.GetType());
        return false;
    }

    MAID localMaid = templateMaid;
    meAssetRequestLoad(&localMaid, 1);
    meAssetWaitUntilLoadstage({ &localMaid, 1 }, Loaded);
    meAsset* tmpl = meAssetTryGet(templateMaid);
    if (!tmpl || tmpl->loadStage != Loaded)
    {
        LOG_ERROR("Failed to load template asset for override deserialize");
        return false;
    }
    void* templateData = loader->resourcePool->GetOpaque(tmpl->runtimeHandle);

    // Allocate the instance and fill it in with (template + overrides).
    Eye instanceEye = loader->resourcePool->Load({.resourceType = meResourceType_InstanceAsset});
    void* instanceData = loader->resourcePool->GetOpaque(instanceEye);
    meSpan instanceBuffer = meSpan(instanceData, loader->assetTypeDesc->size);

    meSerializeResult res = DeserializeOverridesFromTextBlocking(
        *loader->assetTypeDesc,
        ctx.externalDataAllocator,
        inText,
        templateData,
        instanceBuffer);
    if (!res)
    {
        loader->resourcePool->Destroy(instanceEye);
        return false;
    }

    outAsset->id = templateMaid;
    outAsset->runtimeHandle = instanceEye;
    outAsset->loadStage = Loaded;
    return true;
}
