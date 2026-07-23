#include "me_serialize.h"

#include "reflector/reflection_types.h"
#include "platform/me_os.h"
#include "core/me_math.h"
#include "core/containers/dynarray.h"
#include "asset/me_asset.h"
#include "asset/me_asset_index.h"
#include "core/me_chunker.h"
#include "core/me_scope_exit.h"

static bool ChunkWithTypeDescriptor(
	const meTypeDescriptor& td,
	meChunker& chunker,
	void* data,
	meAllocator* readAllocator,
	const meTypeDescriptor* parentType,
	meSerializeResult* outResult);

static u32 meSerializeTypeNameHash(const meTypeDescriptor& typeDesc)
{
	return HashBytes((u8*)typeDesc.name.data, (u32)typeDesc.name.len);
}

bool meSerializeTryReadBinaryHeader(
	meSpan serializedBuffer,
	meSerializedHeader* outHeader)
{
	if (serializedBuffer.size < TD_MESERIALIZEDHEADER.size)
	{
		return false;
	}
	const meSerializedHeader& header = *(const meSerializedHeader*)serializedBuffer.data;
	if (header.magic != ME_BINARY_SERIALIZED_MAGIC ||
		header.formatVersion != ME_BINARY_SERIALIZED_VERSION)
	{
		return false;
	}
	if (outHeader)
	{
		*outHeader = header;
	}
	return true;
}

static bool PrepareSerializedHeaderInAssetData(
	const meTypeDescriptor& typeDesc,
	meSpan assetData)
{
	meSpan serializedHeader = meSerializeTryGetSerializedHeader(typeDesc, assetData);
	if (!serializedHeader)
	{
		return false;
	}

	meSerializedHeader& header = *(meSerializedHeader*)serializedHeader.data;
	MAID assetHeader = header.assetHeader;
	meSpan assetHeaderSpan = meSerializeTryGetAssetHeader(typeDesc, assetData);
	if (assetHeaderSpan)
	{
		assetHeader = *(MAID*)assetHeaderSpan.data;
	}

	header.magic = ME_BINARY_SERIALIZED_MAGIC;
	header.formatVersion = ME_BINARY_SERIALIZED_VERSION;
	header.typeVersion = typeDesc.version;
	header.typeNameHash = meSerializeTypeNameHash(typeDesc);
	header.payloadSize = 0;
	header.assetHeader = assetHeader;
	return true;
}

static bool IsSerializedHeaderField(const meTypeDescriptor& field)
{
	return field.thisType == &TD_MESERIALIZEDHEADER &&
		StringCompare(field.name, STRING_LIT(ME_ASSET_HEADER_FIELDNAME));
}

static bool IsAssetHeaderField(const meTypeDescriptor& field)
{
	return StringCompare(field.name, STRING_LIT(ME_ASSET_HEADER_FIELDNAME)) &&
		(field.thisType == &TD_MESERIALIZEDHEADER || field.thisType == &TD_MAID);
}

static bool TypeHasSerializedHeader(const meTypeDescriptor& typeDesc)
{
	bool result = false;
	meTypeDescriptorWalkMembers(typeDesc, nullptr,
		[&](const meTypeDescriptorMember& member)
		{
			result = IsSerializedHeaderField(member.field);
			return !result;
		},
		false);
	return result;
}

static bool ValidateSerializedHeader(
	const meSerializedHeader& header,
	const meTypeDescriptor& typeDesc,
	meSerializeResult& outResult)
{
	if (header.magic != ME_BINARY_SERIALIZED_MAGIC ||
		header.formatVersion != ME_BINARY_SERIALIZED_VERSION)
	{
		outResult.result = meSerializeResult::SER_FAILURE;
		return false;
	}
	if (header.typeNameHash != meSerializeTypeNameHash(typeDesc))
	{
		LOG_ERROR("Type mismatch deserializing. Expected %.*s", STRING_VAARGS(typeDesc.name));
		outResult.result = meSerializeResult::SER_FAILURE;
		return false;
	}
	if (header.typeVersion != typeDesc.version)
	{
		LOG_ERROR("Version mismatch deserializing type %.*s. Expected version %d but got version %d",
			STRING_VAARGS(typeDesc.name), typeDesc.version, header.typeVersion);
		outResult.result = meSerializeResult::SER_VERSION_MISMATCH;
		return false;
	}
	return true;
}

static void SerializeTopLevelPrologue(const SerializeContext& ctx, bool expectsTemplateData)
{
	ME_ASSERT(ctx.typeDesc);
	ME_ASSERT(ctx.allocator);
	ME_ASSERT(ctx.sourceData.data);
	ME_ASSERT(ctx.sourceData.size == ctx.typeDesc->size);
	ME_ASSERT(!ctx.serializedData);
	// Top-level callers produce serializedData; field walkers use chunker/parentType instead.
	ME_ASSERT(!ctx.chunker);
	ME_ASSERT(!ctx.parentType);
	ME_ASSERT(expectsTemplateData ? ctx.templateData != nullptr : ctx.templateData == nullptr);
}

static void DeserializeTopLevelPrologue(const DeserializeContext& ctx, bool expectsTemplateData)
{
	ME_ASSERT(ctx.typeDesc);
	ME_ASSERT(ctx.outResult);
	ME_ASSERT(ctx.sourceData.data);
	ME_ASSERT(ctx.outputData.data);
	ME_ASSERT(ctx.outputData.size == ctx.typeDesc->size);
	// Top-level callers read sourceData; field walkers use chunker/parentType instead.
	ME_ASSERT(!ctx.chunker);
	ME_ASSERT(!ctx.parentType);
	ME_ASSERT(expectsTemplateData ? ctx.templateData != nullptr : ctx.templateData == nullptr);
}

static bool ChunkDynArray(
	const meTypeDescriptor& td,
	meChunker& chunker,
	void* data,
	meAllocator* readAllocator,
	const meTypeDescriptor* parentType,
	meSerializeResult* outResult)
{
	const meTypeDescriptor* fieldDesc = parentType ? parentType : &td;
	ME_ASSERT(fieldDesc->templatedTypes && fieldDesc->templatedTypes.size == 1);
	const meTypeDescriptor& elemType = *fieldDesc->templatedTypes[0];
	DynArrayAny& arr = *(DynArrayAny*)data;
	u32 count = DynArrayGetSize(arr);
	if (!chunker.BeginArray(count))
	{
		return false;
	}
	ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndArray(); });
	if (chunker.IsReadMode())
	{
		ME_ASSERT(readAllocator);
		if (arr)
		{
			DynArrayDestroy(arr);
		}
		arr = DynArrayCreate<u8>(readAllocator, MEMAX(count, (u32)1), elemType.size);
		arr.header.size = count;
	}
	for (u32 i = 0; i < count; i++)
	{
		void* elemData = arr.data + ((u64)i * elemType.size);
		if (chunker.IsReadMode() && elemType.setToDefaultsFn)
		{
			elemType.setToDefaultsFn(elemData);
		}
		if (!chunker.Element(i))
		{
			return false;
		}
		ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndElement(); });
		if (!ChunkWithTypeDescriptor(elemType, chunker, elemData, readAllocator, fieldDesc, outResult))
		{
			return false;
		}
	}
	return true;
}

static bool ChunkWithTypeDescriptor(
	const meTypeDescriptor& td,
	meChunker& chunker,
	void* data,
	meAllocator* readAllocator,
	const meTypeDescriptor* parentType,
	meSerializeResult* outResult)
{
	if (!data || !td.ShouldSerialize())
	{
		return true;
	}

	if (chunker.IsReadMode() && td.deserializerFn)
	{
		DeserializeContext ctx = {};
		ctx.mode = chunker.GetSerializationMode();
		ctx.typeDesc = &td;
		ctx.outputData = meSpan(data, td.size);
		ctx.chunker = &chunker;
		ctx.externalDataAllocator = readAllocator;
		ctx.parentType = parentType ? parentType : &td;
		ctx.outResult = outResult;
		return td.deserializerFn(td, ctx);
	}
	if (!chunker.IsReadMode() && td.serializerFn)
	{
		SerializeContext ctx = {};
		ctx.mode = chunker.GetSerializationMode();
		ctx.typeDesc = &td;
		ctx.allocator = readAllocator;
		ctx.sourceData = meSpan(data, td.size);
		ctx.chunker = &chunker;
		ctx.parentType = parentType ? parentType : &td;
		return td.serializerFn(td, ctx);
	}
	if (td.thisType && TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray))
	{
		u32 fixedCount = td.thisType->size ? td.size / td.thisType->size : 0;
		u32 count = fixedCount;
		if (!chunker.BeginFixedArray(count))
		{
			return false;
		}
		ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndArray(); });
		if (count != fixedCount)
		{
			return false;
		}
		for (u32 i = 0; i < fixedCount; i++)
		{
			if (!chunker.Element(i))
			{
				return false;
			}
			ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndElement(); });
			void* elementData = data ? (u8*)data + (td.thisType->size * i) : nullptr;
			if (!ChunkWithTypeDescriptor(*td.thisType, chunker, elementData, readAllocator, &td, outResult))
			{
				return false;
			}
		}
		return true;
	}
	if (td.thisType && td.fields.size == 0)
	{
		return ChunkWithTypeDescriptor(*td.thisType, chunker, data, readAllocator, &td, outResult);
	}
	if (td.fields.size > 0)
	{
		if (!chunker.BeginObject())
		{
			return false;
		}
		ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndObject(); });
		bool result = meTypeDescriptorWalkMembers(td, data,
			[&](const meTypeDescriptorMember& member)
			{
				if (!chunker.Field(member.field.name))
				{
					return true;
				}
				ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndField(); });
				return ChunkWithTypeDescriptor(member.field, chunker, member.data, readAllocator, &td, outResult);
			});
		return result;
	}
	ME_ASSERT(!"No serializer for type descriptor");
	return false;
}

static bool TryReadSerializedHeaderField(
	meSerializationMode mode,
	meSpan sourceData,
	meAllocator* allocator,
	meSerializedHeader& header)
{
	meChunker chunker(mode, meChunker::Pass::Read, allocator, sourceData);
	if (!chunker.IsValid() || !chunker.BeginObject())
	{
		return false;
	}
	ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndObject(); });
	if (!chunker.Field(STRING_LIT(ME_ASSET_HEADER_FIELDNAME)))
	{
		return false;
	}
	ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndField(); });
	return ChunkWithTypeDescriptor(TD_MESERIALIZEDHEADER, chunker, &header, allocator, nullptr, nullptr) &&
		chunker.IsReadMode();
}

static bool SerializeChunkerPass(
	SerializeContext& ctx,
	meChunker::Pass mode)
{
	const meTypeDescriptor& typeDesc = *ctx.typeDesc;
	PrepareSerializedHeaderInAssetData(typeDesc, ctx.sourceData);
	meChunker chunker(ctx.mode, mode, ctx.allocator, ctx.serializedData);
	if (!ChunkWithTypeDescriptor(typeDesc, chunker, ctx.sourceData.data, ctx.allocator, nullptr, nullptr))
	{
		return false;
	}
	if (!chunker.IsValid())
	{
		return false;
	}
	if (mode == meChunker::Pass::Write && !chunker.IsWriteMode())
	{
		return false;
	}
	ctx.serializedData = chunker.Finalize(ctx.allocator);
	return true;
}

static meSerializeResult SerializeChunkedBlocking(SerializeContext& ctx)
{
	if (!SerializeChunkerPass(ctx, meChunker::Pass::Measure) ||
		ctx.serializedData.size == 0)
	{
		return meSerializeResult::SER_FAILURE;
	}
	u64 measuredSize = ctx.serializedData.size;
	ctx.serializedData = MEALLOC(ctx.allocator, measuredSize);
	if (!SerializeChunkerPass(ctx, meChunker::Pass::Write))
	{
		return meSerializeResult::SER_FAILURE;
	}
	ME_ASSERT(ctx.serializedData.size == measuredSize);
	return meSerializeResult::SER_SUCCESS;
}

static bool DeserializeChunkedBlocking(DeserializeContext& ctx)
{
	const meTypeDescriptor& typeDesc = *ctx.typeDesc;
	meSerializeResult& outResult = *ctx.outResult;
	outResult.result = meSerializeResult::SER_FAILURE;
	ME_ASSERT(ctx.outputData.size == typeDesc.size);
	if (typeDesc.setToDefaultsFn)
	{
		typeDesc.setToDefaultsFn(ctx.outputData.data);
	}

	meAllocator* chunkerAllocator = ctx.externalDataAllocator ? ctx.externalDataAllocator : GetTLScratch();
	if (TypeHasSerializedHeader(typeDesc))
	{
		meSerializedHeader header = {};
		if (!TryReadSerializedHeaderField(ctx.mode, ctx.sourceData, chunkerAllocator, header) ||
			!ValidateSerializedHeader(header, typeDesc, outResult))
		{
			return false;
		}
	}

	meChunker chunker(ctx.mode, meChunker::Pass::Read, chunkerAllocator, ctx.sourceData);
	bool ok = chunker.IsValid() &&
		ChunkWithTypeDescriptor(typeDesc, chunker, ctx.outputData.data, ctx.externalDataAllocator, nullptr, &outResult) &&
		chunker.IsReadMode();
	outResult.serializedUniqueIdentifier = HashBytesL((u8*)ctx.sourceData.data, ctx.sourceData.size);
	if (ok)
	{
		outResult.result = meSerializeResult::SER_SUCCESS;
	}
	return ok;
}

bool sizedBufferSerializer(
	const meTypeDescriptor& typedescriptor,
	SerializeContext& ctx)
{
	ME_ASSERT(ctx.chunker);
	meSpan& span = *(meSpan*)ctx.sourceData.data;
	ctx.chunker->DoBytes(span);
	return true;
}

bool sizedBufferDeserializer(
	const meTypeDescriptor& typedescriptor,
	DeserializeContext& ctx)
{
	meSpan* outputSpan = (meSpan*)ctx.outputData.data;
	ME_ASSERT(ctx.chunker);
	if (ctx.chunker->IsReadMode() && outputSpan->data)
	{
		ME_ASSERT(ctx.externalDataAllocator);
		MEFREE(ctx.externalDataAllocator, outputSpan->data);
		*outputSpan = {};
	}
	ctx.chunker->DoBytes(*outputSpan, ctx.externalDataAllocator);
	ctx.outputDataExternal = *outputSpan;
	return true;
}

static meAllocator* meStringDeserializerAllocatorOrDefault(const DeserializeContext& ctx)
{
	return ctx.externalDataAllocator ? ctx.externalDataAllocator : GetStringAllocator();
}

bool stringSerializer(
	const meTypeDescriptor& typedescriptor,
	SerializeContext& ctx)
{
	String& str = *(String*)ctx.sourceData.data;
	ME_ASSERT(ctx.chunker);
	ctx.chunker->DoString(str);
	return true;
}

bool stringDeserializer(
	const meTypeDescriptor& typedescriptor,
	DeserializeContext& ctx)
{
	meAllocator* allocator = meStringDeserializerAllocatorOrDefault(ctx);
	String* ownedStr = (String*)ctx.outputData.data;
	ME_ASSERT(ctx.chunker);
	ctx.chunker->DoString(*ownedStr, allocator);
	ctx.outputDataExternal = meSpan(ownedStr->data, ownedStr->len);
	ctx.outputData = meSpan(ownedStr, sizeof(String));
	return true;
}

static bool ChunkFloatArray(meChunker& chunker, float* values, u32 count)
{
	u32 arrayCount = count;
	if (!chunker.BeginFixedArray(arrayCount))
	{
		return false;
	}
	ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndArray(); });
	if (arrayCount != count)
	{
		return false;
	}
	for (u32 i = 0; i < count; i++)
	{
		if (!chunker.Element(i))
		{
			return false;
		}
		ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndElement(); });
		chunker.Do(values[i]);
	}
	return true;
}

static bool ChunkPrimitiveWithTypeDescriptor(
	const meTypeDescriptor& td,
	meChunker& chunker,
	void* data)
{
	if (&td == &TD_INT)                  { chunker.Do(*(s32*)data); return true; }
	else if (&td == &TD_UNSIGNED_INT)    { chunker.Do(*(u32*)data); return true; }
	else if (&td == &TD_LONG_LONG)       { chunker.Do(*(s64*)data); return true; }
	else if (&td == &TD_UNSIGNED_LONG_LONG) { chunker.Do(*(u64*)data); return true; }
	else if (&td == &TD_LONG)            { chunker.Do(*(long*)data); return true; }
	else if (&td == &TD_UNSIGNED_LONG)   { chunker.Do(*(unsigned long*)data); return true; }
	else if (&td == &TD_SHORT)           { chunker.Do(*(s16*)data); return true; }
	else if (&td == &TD_UNSIGNED_SHORT)  { chunker.Do(*(u16*)data); return true; }
	else if (&td == &TD_CHAR)            { chunker.Do(*(s8*)data); return true; }
	else if (&td == &TD_UNSIGNED_CHAR)   { chunker.Do(*(u8*)data); return true; }
	else if (&td == &TD_WCHAR_T)         { chunker.Do(*(wchar_t*)data); return true; }
	else if (&td == &TD_FLOAT)           { chunker.Do(*(float*)data); return true; }
	else if (&td == &TD_DOUBLE)          { chunker.Do(*(double*)data); return true; }
	else if (&td == &TD_BOOL)            { chunker.Do(*(bool*)data); return true; }
	else if (&td == &TD_VEC3)            { return ChunkFloatArray(chunker, (float*)data, 3); }
	else if (&td == &TD_QUAT)            { return ChunkFloatArray(chunker, (float*)data, 4); }
	ME_ASSERT(!"Unhandled primitive serializer type");
	return false;
}

bool primitiveSerializer(
	const meTypeDescriptor& td,
	SerializeContext& ctx)
{
	ME_ASSERT(ctx.chunker);
	return ChunkPrimitiveWithTypeDescriptor(td, *ctx.chunker, ctx.sourceData.data);
}

bool primitiveDeserializer(
	const meTypeDescriptor& td,
	DeserializeContext& ctx)
{
	ME_ASSERT(ctx.chunker);
	return ChunkPrimitiveWithTypeDescriptor(td, *ctx.chunker, ctx.outputData.data);
}

bool serializationDisallowed(
	const meTypeDescriptor& td,
	SerializeContext& ctx)
{
	LOG_ERROR("Serialization is disallowed for type %.*s", STRING_VAARGS(td.name));
	ME_ASSERT(!"Serialization is disallowed for this type descriptor");
	return false;
}

bool deserializationDisallowed(
	const meTypeDescriptor& td,
	DeserializeContext& ctx)
{
	LOG_ERROR("Deserialization is disallowed for type %.*s", STRING_VAARGS(td.name));
	ME_ASSERT(!"Deserialization is disallowed for this type descriptor");
	return false;
}
// Override serialization
// only includes fields whose value differs between instanceData and
// templateData. The asset header (MAID field named "header") is always
// written so the deserializer can locate the template.
static bool ChunkOverridesWithTypeDescriptor(
	const meTypeDescriptor& typeDesc,
	meChunker& chunker,
	void* instanceData,
	const void* templateData,
	meAllocator* allocator,
	meSerializeResult* outResult)
{
	ME_ASSERT(chunker.GetSerializationMode() == meSerializationMode_Text);
	if (!chunker.BeginObject())
	{
		return false;
	}
	ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndObject(); });
	bool result = meTypeDescriptorWalkMembers(typeDesc, instanceData,
		[&](const meTypeDescriptorMember& member)
		{
			void* templateField = (u8*)templateData + member.offsetBytes;
			bool isHeader = IsAssetHeaderField(member.field);
			if (!chunker.IsReadMode() &&
				!isHeader &&
				meFieldsEqual(member.field, member.data, templateField, &typeDesc))
			{
				return true;
			}

			if (!chunker.Field(member.field.name))
			{
				return true;
			}
			ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndField(); });
			return ChunkWithTypeDescriptor(member.field, chunker, member.data, allocator, &typeDesc, outResult);
		});
	return result;
}

static bool ChunkOverrideDocumentWithTypeDescriptor(
	const meTypeDescriptor& typeDesc,
	meChunker& chunker,
	meSpan sourceData,
	const void* templateData,
	meAllocator* allocator,
	meSerializeResult* outResult);

static bool SerializeOverridesPass(
	SerializeContext& ctx,
	meChunker::Pass pass)
{
	meChunker chunker(meSerializationMode_Text, pass, ctx.allocator, ctx.serializedData);
	bool ok = ChunkOverrideDocumentWithTypeDescriptor(
		*ctx.typeDesc,
		chunker,
		ctx.sourceData,
		ctx.templateData,
		ctx.allocator,
		nullptr);
	if (ok)
	{
		ctx.serializedData = chunker.Finalize(ctx.allocator);
	}
	return ok;
}

static bool ChunkOverrideDocumentWithTypeDescriptor(
	const meTypeDescriptor& typeDesc,
	meChunker& chunker,
	meSpan sourceData,
	const void* templateData,
	meAllocator* allocator,
	meSerializeResult* outResult)
{
	PrepareSerializedHeaderInAssetData(typeDesc, sourceData);
	return ChunkOverridesWithTypeDescriptor(
		typeDesc,
		chunker,
		sourceData.data,
		templateData,
		allocator,
		outResult);
}

meSerializeResult SerializeOverridesBlocking(SerializeContext& ctx)
{
	SerializeTopLevelPrologue(ctx, true);

	if (ctx.mode == meSerializationMode_Binary)
	{
		LOG_ERROR("Binary override serialization is not implemented yet");
		return meSerializeResult::SER_FAILURE;
	}
	if (!SerializeOverridesPass(ctx, meChunker::Pass::Measure) ||
		ctx.serializedData.size == 0)
	{
		return meSerializeResult::SER_FAILURE;
	}
	u64 measuredSize = ctx.serializedData.size;
	ctx.serializedData = MEALLOC(ctx.allocator, measuredSize);
	if (!SerializeOverridesPass(ctx, meChunker::Pass::Write))
	{
		return meSerializeResult::SER_FAILURE;
	}
	ME_ASSERT(ctx.serializedData.size == measuredSize);
	return meSerializeResult::SER_SUCCESS;
}

void DeserializeOverridesBlocking(DeserializeContext& ctx)
{
	ME_ASSERT(ctx.outResult);
	if (ctx.mode == meSerializationMode_Binary)
	{
		LOG_ERROR("Binary override deserialization is not implemented yet");
		ctx.outResult->result = meSerializeResult::SER_FAILURE;
		return;
	}
	DeserializeTopLevelPrologue(ctx, true);
	const meTypeDescriptor& typeDesc = *ctx.typeDesc;
	meSerializeResult& outResult = *ctx.outResult;
	outResult.result = meSerializeResult::SER_FAILURE;
	meAllocator* textStateAllocator = ctx.externalDataAllocator ? ctx.externalDataAllocator : GetTLScratch();

	meChunker chunker(meSerializationMode_Text, meChunker::Pass::Read, textStateAllocator, ctx.sourceData);
	if (!chunker.IsValid())
	{
		outResult.result = meSerializeResult::SER_FAILURE;
		return;
	}

	if (TypeHasSerializedHeader(typeDesc))
	{
		meSerializedHeader header = {};
		if (!TryReadSerializedHeaderField(ctx.mode, ctx.sourceData, textStateAllocator, header) ||
			!ValidateSerializedHeader(header, typeDesc, outResult))
		{
			return;
		}
	}

	// Overrides are sparse, so missing fields intentionally keep their template values.
	// Copy the template first, then apply the serialized overrides on top.
	DeepCopyContext copyCtx = {};
	copyCtx.srcData = ctx.templateData;
	copyCtx.outputData = ctx.outputData;
	copyCtx.allocator = textStateAllocator;
	meTypeDescriptorDeepCopy(typeDesc, copyCtx);

	bool ok = ChunkOverridesWithTypeDescriptor(
		typeDesc,
		chunker,
		ctx.outputData.data,
		ctx.templateData,
		textStateAllocator,
		&outResult);
	if (!ok || !chunker.IsReadMode())
	{
		outResult.result = meSerializeResult::SER_FAILURE;
		return;
	}
	outResult.result = meSerializeResult::SER_SUCCESS;
}


void DeserializeFromFileBlocking(
	StringView filepath,
	DeserializeContext& ctx)
{
	Allocation tempFileContent = {};
    OSFileReference file;
    if (!meOSOpenFile(file, filepath, (OSFileFlags_OnlyIfExists | OSFileFlags_ScopedFile | OSFileFlags_ReadOnly))) // TODO: memmap the file instead
    {
        if (ctx.outResult) *ctx.outResult = meSerializeResult::SER_FAILURE;
        return;
    }
    tempFileContent = MEALLOC(GetTLScratch(), meOSGetFileSize(file));
    meOSReadFileContents(file, tempFileContent, tempFileContent.size);

	ctx.sourceData = tempFileContent;
	DeserializeBlocking(ctx);
	// TODO: could/should be replaced with timestamp
	if (ctx.outResult)
	{
		ctx.outResult->serializedUniqueIdentifier = HashBytesL((u8*)tempFileContent.data, tempFileContent.size);
	}
}

meSerializeResult SerializeBlocking(SerializeContext& ctx)
{
	SerializeTopLevelPrologue(ctx, false);
	return SerializeChunkedBlocking(ctx);
}

void DeserializeBlocking(DeserializeContext& ctx)
{
	DeserializeTopLevelPrologue(ctx, false);
	DeserializeChunkedBlocking(ctx);
}



// =========================================================



bool DynArraySerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext& ctx)
{
	const meTypeDescriptor* parentType = ctx.parentType;
	// DynArray is templated, and so requires the parent type to understand the template args, see comment in meTypeDescriptor struct
	ME_ASSERT(parentType);
	ME_ASSERT(ctx.chunker);
	return ChunkDynArray(typeDescriptor, *ctx.chunker, ctx.sourceData.data, ctx.allocator, parentType, nullptr);
}

bool DynArrayDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx)
{
	const meTypeDescriptor* parentType = ctx.parentType;
	// DynArray is templated, and so requires the parent type to understand the template args, see comment in meTypeDescriptor struct
	ME_ASSERT(parentType);
	ME_ASSERT(ctx.chunker);
	return ChunkDynArray(typeDescriptor, *ctx.chunker, ctx.outputData.data, ctx.externalDataAllocator, parentType, ctx.outResult);
}

static bool meAssetIsLoadedInstance(const meAsset& asset)
{
	return asset.runtimeHandle
		&& !asset.runtimeHandle.IsTemplateAsset()
		&& asset.loadStage == Loaded
		&& asset.id;
}

static bool ChunkAssetReference(meChunker& chunker, MAID& maid)
{
	if (!chunker.BeginObject())
	{
		return false;
	}
	ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndObject(); });

	u64 id = maid.GetID();
	u32 type = (u32)maid.GetType();
	bool ok = true;
	if (!chunker.Field(STRING_LIT("id")))
	{
		ok = false;
	}
	else
	{
		ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndField(); });
		chunker.Do(id);
	}

	if (ok && !chunker.Field(STRING_LIT("type")))
	{
		ok = false;
	}
	else if (ok)
	{
		ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndField(); });
		chunker.Do(type);
	}

	if (ok && chunker.IsReadMode())
	{
		maid.SetID(id);
		maid.SetType((meAssetType)type);
	}
	return ok;
}

bool meAssetSerializerFn(const meTypeDescriptor& td, SerializeContext& ctx)
{
    meAsset* asset = (meAsset*)ctx.sourceData.data;
	ME_ASSERT(ctx.chunker);

    bool isInstance = meAssetIsLoadedInstance(*asset);
    if (!isInstance)
    {
		MAID assetId = asset->id;
		return ChunkAssetReference(*ctx.chunker, assetId);
	}

	// Binary asset override serialization is not implemented yet.
	if (ctx.chunker->GetSerializationMode() != meSerializationMode_Text)
	{
		LOG_ERROR("Binary asset override serialization is not implemented yet");
		return false;
	}

    meAssetLoader* loader = meAssetSystemGet().assetLoaders[asset->id.GetType()];
    ME_ASSERT(loader);

    MAID templateMaid = asset->id;
    meAssetRequestLoadTemplate(&templateMaid, 1);
    meAssetWaitUntilLoadstage({ &templateMaid, 1 }, Loaded);
    meAsset* tmpl = meAssetTryGetTemplate(templateMaid);
    ME_ASSERT(tmpl);
    if (!tmpl->runtimeHandle.IsTemplateAsset())
    {
        // an instance asset *created from a template* will have the template asset MAID
        // an instance asset *created at runtime* (and therefore NOT derived from a template asset)
        // shouldn't be serialized at all
        return false;
    }

    void* templateData = loader->resourcePool->GetOpaque(tmpl->runtimeHandle);
    void* instanceData = loader->resourcePool->GetOpaque(asset->runtimeHandle);

	return ChunkOverrideDocumentWithTypeDescriptor(
		*loader->assetTypeDesc,
		*ctx.chunker,
		meSpan(instanceData, loader->assetTypeDesc->size),
		templateData,
		ctx.allocator,
		nullptr);
}

bool meAssetDeserializerFn(const meTypeDescriptor& td, DeserializeContext& ctx)
{
    meAsset* outAsset = (meAsset*)ctx.outputData.data;
    ME_ASSERT(ctx.chunker);
	bool isOverrideDoc = meAssetIsLoadedInstance(*outAsset);
    *outAsset = meAsset();

    MAID templateMaid = {};
    if (isOverrideDoc)
    {
		// Binary asset override deserialization is not implemented yet.
		if (ctx.chunker->GetSerializationMode() != meSerializationMode_Text)
		{
			LOG_ERROR("Binary asset override deserialization is not implemented yet");
			return false;
		}

		meSerializedHeader header = {};
		if (!ctx.chunker->BeginObject())
		{
			LOG_ERROR("meAsset override data must be a serialized override document");
			return false;
		}
		ME_ON_SCOPE_EXIT([&ctx]() { ctx.chunker->EndObject(); });
		if (!ctx.chunker->Field(STRING_LIT(ME_ASSET_HEADER_FIELDNAME)))
		{
			LOG_ERROR("meAsset override data must be a serialized override document");
			return false;
		}
		ME_ON_SCOPE_EXIT([&ctx]() { ctx.chunker->EndField(); });
		if (!ChunkWithTypeDescriptor(TD_MESERIALIZEDHEADER, *ctx.chunker, &header, ctx.externalDataAllocator, nullptr, nullptr) ||
			header.magic != ME_BINARY_SERIALIZED_MAGIC ||
			header.formatVersion != ME_BINARY_SERIALIZED_VERSION)
		{
			LOG_ERROR("meAsset override data must be a serialized override document");
			return false;
		}
        templateMaid = header.assetHeader;
    }
    else
    {
		if (!ChunkAssetReference(*ctx.chunker, templateMaid))
		{
			LOG_ERROR("meAsset reference malformed");
			return false;
		}
    }

    outAsset->id = templateMaid;
    outAsset->runtimeHandle = EYE_INVALID;
    outAsset->loadStage = Unloaded;
    if (ctx.outResult)
    {
        meAssetIndexRecordDependency(ctx.outResult->ownerMaid, templateMaid);
    }
    return true;
}

bool meAssetEqualsFn(const meTypeDescriptor& td, const void* a, const void* b)
{
    const meAsset* assetA = (const meAsset*)a;
    const meAsset* assetB = (const meAsset*)b;

    if (assetA->id != assetB->id) 
    {
        return false;
    }

    // Determine whether each side is a loaded instance (vs. a plain template ref).
    // Mirrors the isInstance check in meAssetSerializerFn.
    bool aIsInstance = assetA->runtimeHandle
                    && !assetA->runtimeHandle.IsTemplateAsset()
                    && assetA->loadStage == Loaded;
    bool bIsInstance = assetB->runtimeHandle
                    && !assetB->runtimeHandle.IsTemplateAsset()
                    && assetB->loadStage == Loaded;

    if (aIsInstance != bIsInstance) 
    {
        return false;
    }

    // Both are plain template refs with matching IDs — equal.
    if (!aIsInstance) 
    {
        return true;
    }

    // Both are loaded instances of the same template.
    // The same Eye means the same resource-pool slot, so identical data.
    // Alternative is to do more depthy comparison, but in practice i don't think that's necessary.
    // NOTE: that does also mean two separate loaded runtime handles with identical content will compare false. That's fine though.
    return assetA->runtimeHandle == assetB->runtimeHandle;
}
