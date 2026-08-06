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
	meSerializeResult* outResult,
	DynArray<MAID>* assetDependencies = nullptr,
	bool resetPresentFieldsBeforeRead = false);

static bool TryReadSerializedHeaderField(
	meSerializationMode mode,
	meSpan sourceData,
	meAllocator* allocator,
	meSerializedHeader& header);

static bool ValidateSerializedHeader(
	const meSerializedHeader& header,
	const meTypeDescriptor& typeDesc,
	meSerializeResult& outResult);

static u32 meSerializeTypeNameHash(const meTypeDescriptor& typeDesc)
{
	return HashBytes((u8*)typeDesc.name.data, (u32)typeDesc.name.len);
}

static bool SerializedBufferStartsWithBinaryHeader(meSpan serializedBuffer)
{
	if (serializedBuffer.size < offsetof(meSerializedHeader, typeVersion))
	{
		return false;
	}

	const meSerializedHeader* header = (const meSerializedHeader*)serializedBuffer.data;
	return header->magic == ME_BINARY_SERIALIZED_MAGIC &&
		header->formatVersion == ME_BINARY_SERIALIZED_VERSION;
}

bool meSerializeTryReadBinaryHeader(
	meSpan serializedBuffer,
	meSerializedHeader* outHeader)
{
	if (!SerializedBufferStartsWithBinaryHeader(serializedBuffer))
	{
		return false;
	}

	meSerializedHeader header = {};
	meAllocator* allocator = GetTLScratch();
	if (!TryReadSerializedHeaderField(meSerializationMode_Binary, serializedBuffer, allocator, header))
	{
		return false;
	}
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
	meSpan assetData,
	bool resetDependencies)
{
	meSpan serializedHeader = meSerializeTryGetSerializedHeader(typeDesc, assetData);
	if (!serializedHeader)
	{
		return false;
	}

	meSerializedHeader& header = *(meSerializedHeader*)serializedHeader.data;
	MAID assetHeader = header.assetHeader;
	header.magic = ME_BINARY_SERIALIZED_MAGIC;
	header.formatVersion = ME_BINARY_SERIALIZED_VERSION;
	header.typeVersion = typeDesc.version;
	header.typeNameHash = meSerializeTypeNameHash(typeDesc);
	header.payloadSize = 0;
	header.assetHeader = assetHeader;
	if (resetDependencies)
	{
		DynArrayClear(header.dependencies);
	}
	return true;
}

bool meSerializeTryReadHeader(
	meSerializationMode mode,
	meSpan serializedBuffer,
	const meTypeDescriptor& typeDesc,
	meAllocator* allocator,
	meSerializedHeader* outHeader)
{
	meSerializedHeader header = {};
	if (mode == meSerializationMode_Binary && !SerializedBufferStartsWithBinaryHeader(serializedBuffer))
	{
		return false;
	}

	meAllocator* readAllocator = allocator ? allocator : GetTLScratch();
	meSerializeResult result = {};
	if (!TryReadSerializedHeaderField(mode, serializedBuffer, readAllocator, header) ||
		!ValidateSerializedHeader(header, typeDesc, result))
	{
		return false;
	}
	if (outHeader)
	{
		*outHeader = header;
	}
	return true;
}

static bool IsSerializedHeaderField(const meTypeDescriptor& field)
{
	return field.thisType == &TD_MESERIALIZEDHEADER &&
		StringCompare(field.name, STRING_LIT(ME_ASSET_HEADER_FIELDNAME));
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

static bool DeepCopyParentAssetIfNeeded(
	const meSerializedHeader& header,
	const meTypeDescriptor& typeDesc,
	DeserializeContext& ctx)
{
	if (!header.parentAsset)
	{
		return false;
	}
	if (header.parentAsset.GetType() != header.assetHeader.GetType())
	{
		LOG_ERROR("Parent asset type does not match child asset type");
		ctx.outResult->result = meSerializeResult::SER_FAILURE;
		return false;
	}

	MAID parentMaid = header.parentAsset;
	meAssetRequestLoadTemplate(&parentMaid, 1);
	if (!meAssetWaitUntilLoadstage({ &parentMaid, 1 }, Loaded))
	{
		LOG_ERROR("Failed to load parent asset while deserializing inherited asset");
		ctx.outResult->result = meSerializeResult::SER_FAILURE;
		return false;
	}

	meAsset* parentAsset = meAssetTryGetTemplate(parentMaid);
	if (!parentAsset || !parentAsset->runtimeHandle)
	{
		LOG_ERROR("Parent asset was not available after load");
		ctx.outResult->result = meSerializeResult::SER_FAILURE;
		return false;
	}

	meAssetLoader* loader = meAssetSystemGet().assetLoaders[parentMaid.GetType()];
	if (!loader || loader->assetTypeDesc != &typeDesc)
	{
		LOG_ERROR("Parent asset loader does not match deserialized asset type");
		ctx.outResult->result = meSerializeResult::SER_FAILURE;
		return false;
	}

    ScopedAssetOpaqueLockR parentData(*parentAsset);
	DeepCopyContext copyCtx = {};
	copyCtx.srcData = parentData.Get();
	copyCtx.outputData = ctx.outputData;
	copyCtx.allocator = ctx.externalDataAllocator;
	meTypeDescriptorDeepCopy(typeDesc, copyCtx);
	return true;
}

static void SerializeTopLevelPrologue(const SerializeContext& ctx)
{
	ME_ASSERT(ctx.typeDesc);
	ME_ASSERT(ctx.allocator);
	ME_ASSERT(ctx.sourceData.data);
	ME_ASSERT(ctx.sourceData.size == ctx.typeDesc->size);
	ME_ASSERT(!ctx.serializedData);
	// Top-level callers produce serializedData; field walkers use chunker/parentType instead.
	ME_ASSERT(!ctx.chunker);
	ME_ASSERT(!ctx.parentType);
}

static void DeserializeTopLevelPrologue(const DeserializeContext& ctx)
{
	ME_ASSERT(ctx.typeDesc);
	ME_ASSERT(ctx.outResult);
	ME_ASSERT(ctx.sourceData.data);
	ME_ASSERT(ctx.outputData.data);
	ME_ASSERT(ctx.outputData.size == ctx.typeDesc->size);
	// Top-level callers read sourceData; field walkers use chunker/parentType instead.
	ME_ASSERT(!ctx.chunker);
	ME_ASSERT(!ctx.parentType);
}

static void SetToDefaultsWithTypeDescriptor(
	const meTypeDescriptor& td,
	void* data)
{
	if (!data)
	{
		return;
	}
	if (td.setToDefaultsFn)
	{
		td.setToDefaultsFn(data);
		return;
	}
	if (td.thisType && TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray))
	{
		u32 fixedCount = td.thisType->size ? td.size / td.thisType->size : 0;
		for (u32 i = 0; i < fixedCount; i++)
		{
			SetToDefaultsWithTypeDescriptor(*td.thisType, (u8*)data + (td.thisType->size * i));
		}
		return;
	}
	if (td.thisType && !TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray) && td.thisType->setToDefaultsFn)
	{
		td.thisType->setToDefaultsFn(data);
	}
}

static void ResetFieldBeforeDeserialize(
	const meTypeDescriptor& field,
	void* data,
	meAllocator* allocator,
	const meTypeDescriptor* parentType)
{
	DestroyContext destroyCtx = {};
	destroyCtx.data = data;
	destroyCtx.allocator = allocator;
	destroyCtx.parentType = parentType;
	meTypeDescriptorDestroy(field, destroyCtx);
	SetToDefaultsWithTypeDescriptor(field, data);
}

static bool ChunkDynArray(
	const meTypeDescriptor& td,
	meChunker& chunker,
	void* data,
	meAllocator* readAllocator,
	const meTypeDescriptor* parentType,
	meSerializeResult* outResult,
	DynArray<MAID>* assetDependencies,
	bool resetPresentFieldsBeforeRead = false)
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
		if (!ChunkWithTypeDescriptor(elemType, chunker, elemData, readAllocator, fieldDesc, outResult, assetDependencies, resetPresentFieldsBeforeRead))
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
	meSerializeResult* outResult,
	DynArray<MAID>* assetDependencies,
	bool resetPresentFieldsBeforeRead)
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
		ctx.assetDependencies = assetDependencies;
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
			if (!ChunkWithTypeDescriptor(*td.thisType, chunker, elementData, readAllocator, &td, outResult, assetDependencies, resetPresentFieldsBeforeRead))
			{
				return false;
			}
		}
		return true;
	}
	if (td.thisType && td.fields.size == 0)
	{
		return ChunkWithTypeDescriptor(*td.thisType, chunker, data, readAllocator, &td, outResult, assetDependencies, resetPresentFieldsBeforeRead);
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
					// In text mode, field presence is the "set" bit. Missing fields keep
					// either the copied parent value or the type default.
					return true;
				}
				ME_ON_SCOPE_EXIT([&chunker]() { chunker.EndField(); });
				if (resetPresentFieldsBeforeRead && chunker.IsReadMode())
				{
					ResetFieldBeforeDeserialize(member.field, member.data, readAllocator, &td);
				}
				return ChunkWithTypeDescriptor(member.field, chunker, member.data, readAllocator, &td, outResult, assetDependencies, false);
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
	meChunker::Pass mode,
	bool resetDependencies)
{
	const meTypeDescriptor& typeDesc = *ctx.typeDesc;
	PrepareSerializedHeaderInAssetData(typeDesc, ctx.sourceData, resetDependencies);
	DynArray<MAID>* dependencies = nullptr;
	meSpan serializedHeader = meSerializeTryGetSerializedHeader(typeDesc, ctx.sourceData);
	if (serializedHeader)
	{
		dependencies = &((meSerializedHeader*)serializedHeader.data)->dependencies;
	}
	meChunker chunker(ctx.mode, mode, ctx.allocator, ctx.serializedData);
	if (!ChunkWithTypeDescriptor(typeDesc, chunker, ctx.sourceData.data, ctx.allocator, nullptr, nullptr, dependencies))
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
	// Dependencies live in the header, but asset references are encountered after the
	// header during the type walk. Do one measure pass to collect dependencies, then
	// measure again with the completed header so allocation size is correct.
    // TODO: serialize an offset pointer for the dependencies list in the header and then fill in the actual dependencies later so we don't need to do this twice.
	if (!SerializeChunkerPass(ctx, meChunker::Pass::Measure, true))
	{
		return meSerializeResult::SER_FAILURE;
	}
	ctx.serializedData = {};
	if (!SerializeChunkerPass(ctx, meChunker::Pass::Measure, false) ||
		ctx.serializedData.size == 0)
	{
		return meSerializeResult::SER_FAILURE;
	}
	u64 measuredSize = ctx.serializedData.size;
	ctx.serializedData = MEALLOC(ctx.allocator, measuredSize);
	if (!SerializeChunkerPass(ctx, meChunker::Pass::Write, false))
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

	meAllocator* chunkerAllocator = ctx.externalDataAllocator ? ctx.externalDataAllocator : GetTLScratch();
	meSerializedHeader header = {};
	bool hasSerializedHeader = TypeHasSerializedHeader(typeDesc);
	bool sourceCanContainHeader = ctx.mode != meSerializationMode_Binary || SerializedBufferStartsWithBinaryHeader(ctx.sourceData);
	bool sourceHasSerializedHeader = false;
	if (sourceCanContainHeader)
	{
		sourceHasSerializedHeader = TryReadSerializedHeaderField(ctx.mode, ctx.sourceData, chunkerAllocator, header);
		if (sourceHasSerializedHeader && !ValidateSerializedHeader(header, typeDesc, outResult))
		{
			return false;
		}
	}
	if (hasSerializedHeader && !sourceHasSerializedHeader)
	{
		return false;
	}

	bool copiedParent = false;
	if (sourceHasSerializedHeader && header.parentAsset)
	{
		copiedParent = DeepCopyParentAssetIfNeeded(header, typeDesc, ctx);
		if (!copiedParent)
		{
			return false;
		}
	}
	if (!copiedParent && typeDesc.setToDefaultsFn)
	{
		typeDesc.setToDefaultsFn(ctx.outputData.data);
	}

	meChunker chunker(ctx.mode, meChunker::Pass::Read, chunkerAllocator, ctx.sourceData);
	bool ok = chunker.IsValid() &&
		ChunkWithTypeDescriptor(typeDesc, chunker, ctx.outputData.data, ctx.externalDataAllocator, nullptr, &outResult, nullptr, copiedParent) &&
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
	SerializeTopLevelPrologue(ctx);
	return SerializeChunkedBlocking(ctx);
}

void DeserializeBlocking(DeserializeContext& ctx)
{
	DeserializeTopLevelPrologue(ctx);
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
	return ChunkDynArray(typeDescriptor, *ctx.chunker, ctx.sourceData.data, ctx.allocator, parentType, nullptr, ctx.assetDependencies);
}

bool DynArrayDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx)
{
	const meTypeDescriptor* parentType = ctx.parentType;
	// DynArray is templated, and so requires the parent type to understand the template args, see comment in meTypeDescriptor struct
	ME_ASSERT(parentType);
	ME_ASSERT(ctx.chunker);
	return ChunkDynArray(typeDescriptor, *ctx.chunker, ctx.outputData.data, ctx.externalDataAllocator, parentType, ctx.outResult, nullptr);
}

static void AddAssetDependency(DynArray<MAID>* dependencies, MAID maid, meAllocator* allocator)
{
	if (!dependencies || !maid)
	{
		return;
	}
	if (!*dependencies)
	{
		*dependencies = DynArrayCreate<MAID>(allocator);
	}
	for (DynArray_Foreach(*dependencies, i))
	{
		if ((*dependencies)[i] == maid)
		{
			return;
		}
	}
	DynArrayPush(*dependencies, maid);
}

bool meAssetSerializerFn(const meTypeDescriptor& td, SerializeContext& ctx)
{
    meAsset* asset = (meAsset*)ctx.sourceData.data;
	ME_ASSERT(ctx.chunker);

	MAID assetId = asset->id;
	AddAssetDependency(ctx.assetDependencies, assetId, ctx.allocator);
	return ChunkWithTypeDescriptor(TD_MAID, *ctx.chunker, &assetId, ctx.allocator, nullptr, nullptr);
}

bool meAssetDeserializerFn(const meTypeDescriptor& td, DeserializeContext& ctx)
{
    meAsset* outAsset = (meAsset*)ctx.outputData.data;
    ME_ASSERT(ctx.chunker);
    *outAsset = meAsset();

    MAID templateMaid = {};
	if (!ChunkWithTypeDescriptor(TD_MAID, *ctx.chunker, &templateMaid, ctx.externalDataAllocator, nullptr, ctx.outResult))
	{
		LOG_ERROR("meAsset reference malformed");
		return false;
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
