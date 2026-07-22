
#include "reflection_types.h"
#include "core/me_string.h"
#include "core/me_math.h"
#include "core/me_log.h"


StringView meTypeDescriptorFlagToString(meTypeDescriptorFlags flag)
{
	switch (flag)
	{
		#define X(name) case meTypeDescriptorFlag_##name: return STRING_LIT(ME_MACRO_STRINGIZE_EX(ME_MACRO_CONCAT(meTypeDescriptorFlag_, name)));
		DECLARE_METYPEDESCRIPTOR_FLAGS
		#undef X
		default: return {};
	};
}

// Serializer/Deserializer functions for sized buffer types (meSpan, StringView, String)
#ifdef ME_CORE_ONLY
// raw fallback for reflector build (no json available)
void sizedBufferSerializer(
	const meTypeDescriptor& typedescriptor,
	SerializeContext& ctx)
{ UNIMPLEMENTED(); }

void stringSerializer(
	const meTypeDescriptor& typedescriptor,
	SerializeContext& ctx)
{ UNIMPLEMENTED(); }

#endif

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

static meAllocator* meStringDeserializerAllocatorOrDefault(const DeserializeContext& ctx)
{
	return ctx.externalDataAllocator ? ctx.externalDataAllocator : GetStringAllocator();
}

bool stringDeserializer(
	const meTypeDescriptor& typedescriptor,
	DeserializeContext& ctx)
{
	meAllocator* allocator = meStringDeserializerAllocatorOrDefault(ctx);
	String* ownedStr = (String*)ctx.outputData.data;
	ownedStr->CopyOf(StringView::FromSpan(ctx.inputData), allocator);
	ownedStr->allocator = allocator;
	ctx.outputDataExternal = meSpan(ownedStr->data, ownedStr->len);
	ctx.outputData = meSpan(ownedStr, sizeof(String));
	return true;
}

bool sizedBufferEquals(
    const meTypeDescriptor& td,
    const void* a, 
    const void* b)
{
	const meSpan* sa = (const meSpan*)a;
	const meSpan* sb = (const meSpan*)b;
	if (sa->size != sb->size) return false;
	if (sa->size == 0) return true;
	if (sa->data == sb->data) return true;
	return memcmp(sa->data, sb->data, sa->size) == 0;
}

bool meFieldsEqual(
	const meTypeDescriptor& td,
	const void* a,
	const void* b,
	const meTypeDescriptor* parentType)
{
	if (!td.ShouldSerialize())
	{
		return true;
	}

	if (td.equalsFn)
	{
		return td.equalsFn(td, a, b);
	}

	if (td.thisType && TEST_BIT(td.flags, meTypeDescriptorFlag_ConstantArray))
	{
		u32 elemSize = td.thisType->size;
		if (elemSize == 0) return memcmp(a, b, td.size) == 0;
		bool elementsEqual = true;
		meTypeDescriptorWalkElements(td, const_cast<void*>(a),
			[&](const meTypeDescriptorMember& element)
			{
				const void* bElem = (const u8*)b + (elemSize * element.index);
				if (!meFieldsEqual(element.field, element.data, bElem, element.parentType))
				{
					elementsEqual = false;
					return false;
				}
				return true;
			});
		return elementsEqual;
	}

	if (td.thisType)
	{
		if (td.thisType->equalsFn)
		{
			return td.thisType->equalsFn(td, a, b);
		}
		return meFieldsEqual(*td.thisType, a, b, &td);
	}

	if (td.fields.size > 0)
	{
		bool fieldsEqual = true;
		meTypeDescriptorWalkMembers(td, const_cast<void*>(a),
			[&](const meTypeDescriptorMember& member)
			{
				const void* bField = (const u8*)b + member.offsetBytes;
				if (!meFieldsEqual(member.field, member.data, bField, &td))
				{
					fieldsEqual = false;
					return false;
				}
				return true;
			});
		return fieldsEqual;
	}

	return memcmp(a, b, td.size) == 0;
}

void meTypeDescriptorDestroy(
	const meTypeDescriptor& typeDesc,
	DestroyContext& ctx)
{
	if (!ctx.data)
	{
		return;
	}

	if (typeDesc.destroyFn)
	{
		typeDesc.destroyFn(typeDesc, ctx);
		return;
	}

	if (typeDesc.thisType && TEST_BIT(typeDesc.flags, meTypeDescriptorFlag_ConstantArray))
	{
		meTypeDescriptorWalkElements(typeDesc, ctx.data,
			[&](const meTypeDescriptorMember& element)
			{
				DestroyContext elementCtx = ctx;
				elementCtx.data = element.data;
				elementCtx.parentType = element.parentType;
				meTypeDescriptorDestroy(element.field, elementCtx);
				return true;
			});
		return;
	}

	if (typeDesc.thisType && typeDesc.fields.size == 0)
	{
		DestroyContext aliasCtx = ctx;
		aliasCtx.parentType = &typeDesc;
		meTypeDescriptorDestroy(*typeDesc.thisType, aliasCtx);
		return;
	}

	meTypeDescriptorWalkMembers(typeDesc, ctx.data,
		[&](const meTypeDescriptorMember& member)
		{
			DestroyContext fieldCtx = ctx;
			fieldCtx.data = member.data;
			fieldCtx.parentType = &typeDesc;
			meTypeDescriptorDestroy(member.field, fieldCtx);
			return true;
		});
}

void sizedBufferDestroy(
	const meTypeDescriptor& typedescriptor,
	DestroyContext& ctx)
{
	meSpan& span = *(meSpan*)ctx.data;
	if (span.data)
	{
		ME_ASSERT(ctx.allocator);
		MEFREE(ctx.allocator, span.data);
		span = {};
	}
}

void stringDestroy(
	const meTypeDescriptor& typedescriptor,
	DestroyContext& ctx)
{
	String* str = (String*)ctx.data;
	str->~String();
	new (str) String();
}

void sizedBufferDeepCopy(
	const meTypeDescriptor& typedescriptor,
	DeepCopyContext& ctx)
{
	const meSpan* srcSpan = (const meSpan*)ctx.srcData;
	meSpan* dstSpan = (meSpan*)ctx.outputData.data;
	if (!srcSpan->data || srcSpan->size == 0)
	{
		*dstSpan = {};
		return;
	}
	Allocation mem = MEALLOC(ctx.allocator, srcSpan->size);
	BufferCopy(mem, *srcSpan);
	*dstSpan = mem;
}

void stringDeepCopy(
	const meTypeDescriptor& typedescriptor,
	DeepCopyContext& ctx)
{
	String* dstString = (String*)ctx.outputData.data;
	const String* srcString = (const String*)ctx.srcData;
	dstString->CopyOf(StringView(*srcString), ctx.allocator);
	dstString->allocator = ctx.allocator;
}

void DynArrayDeepCopyFn(
	const meTypeDescriptor& typeDescriptor,
	DeepCopyContext& ctx)
{
	const meTypeDescriptor* fieldDesc = ctx.parentType ? ctx.parentType : &typeDescriptor;
	DynArrayAny& src = *(DynArrayAny*)ctx.srcData;
	DynArrayAny& dst = *(DynArrayAny*)ctx.outputData.data;
	if (!src)
	{
		dst = {};
		return;
	}

	const meTypeDescriptor& elementType = *meTypeDescriptorGetSingleTemplateArg(*fieldDesc);
	u32 size = DynArrayGetSize(src);
	u32 capacity = DynArrayGetCapacity(src);
	u32 stride = DynArrayGetStride(src);
	dst = DynArrayCreate<u8>(ctx.allocator, MEMAX(capacity, (u32)1), stride);
	dst.header.size = size;
	meTypeDescriptorWalkElements(typeDescriptor, &src,
		[&](const meTypeDescriptorMember& element)
		{
			DeepCopyContext elementCtx = {};
			elementCtx.srcData = element.data;
			elementCtx.outputData = meSpan(dst.data + ((u32)element.key * stride), elementType.size);
			elementCtx.allocator = ctx.allocator;
			elementCtx.parentType = element.parentType;
			meTypeDescriptorDeepCopy(element.field, elementCtx);
			return true;
		},
		fieldDesc);
}

void DynArrayDestroyFn(
	const meTypeDescriptor& typeDescriptor,
	DestroyContext& ctx)
{
	const meTypeDescriptor* fieldDesc = ctx.parentType ? ctx.parentType : &typeDescriptor;
	DynArrayAny& arr = *(DynArrayAny*)ctx.data;
	if (!arr)
	{
		return;
	}

	if (fieldDesc->templatedTypes && fieldDesc->templatedTypes.size == 1)
	{
		meTypeDescriptorWalkElements(typeDescriptor, &arr,
			[&](const meTypeDescriptorMember& element)
			{
				DestroyContext elementCtx = ctx;
				elementCtx.data = element.data;
				elementCtx.parentType = element.parentType;
				meTypeDescriptorDestroy(element.field, elementCtx);
				return true;
			},
			fieldDesc);
	}
	DynArrayDestroy(arr);
}

bool DynArrayEqualsFn(
    const meTypeDescriptor& td,
    const void* a,
    const void* b)
{
    // td here is the field descriptor (not TD_DYNARRAY itself), so templatedTypes is populated.
    const meTypeDescriptor& elementType = *meTypeDescriptorGetSingleTemplateArg(td);

    DynArrayAny& arrA = *(DynArrayAny*)a;
    DynArrayAny& arrB = *(DynArrayAny*)b;

    u32 sizeA = DynArrayGetSize(arrA);
    u32 sizeB = DynArrayGetSize(arrB);

    if (sizeA != sizeB) return false;
    if (sizeA == 0) return true;

    u32 stride = DynArrayGetStride(arrA);

    for (u32 i = 0; i < sizeA; i++)
    {
        const void* elemA = &arrA[i * stride];
        const void* elemB = &arrB[i * stride];
        if (!meFieldsEqual(elementType, elemA, elemB, &td))
        {
            return false;
        }
    }
    return true;
}

void meTypeDescriptorDeepCopy(
	const meTypeDescriptor& typeDesc,
	DeepCopyContext& ctx)
{
	if (!ctx.srcData || !ctx.outputData.data)
	{
		return;
	}
	ME_ASSERT(ctx.outputData.size == typeDesc.size);

	if (typeDesc.deepCopyFn)
	{
		typeDesc.deepCopyFn(typeDesc, ctx);
		return;
	}

	if (typeDesc.thisType && TEST_BIT(typeDesc.flags, meTypeDescriptorFlag_ConstantArray))
	{
		meTypeDescriptorWalkElements(typeDesc, const_cast<void*>(ctx.srcData),
			[&](const meTypeDescriptorMember& element)
			{
				DeepCopyContext elementCtx = {};
				elementCtx.srcData = element.data;
				elementCtx.outputData = meSpan((u8*)ctx.outputData.data + (typeDesc.thisType->size * element.index), element.field.size);
				elementCtx.allocator = ctx.allocator;
				elementCtx.parentType = element.parentType;
				meTypeDescriptorDeepCopy(element.field, elementCtx);
				return true;
			});
		return;
	}

	if (typeDesc.thisType && typeDesc.fields.size == 0)
	{
		DeepCopyContext aliasCtx = ctx;
		aliasCtx.parentType = &typeDesc;
		meTypeDescriptorDeepCopy(*typeDesc.thisType, aliasCtx);
		return;
	}

	ME_MEMCPY(ctx.outputData.data, ctx.srcData, typeDesc.size);

	meTypeDescriptorWalkMembers(typeDesc, const_cast<void*>(ctx.srcData),
		[&](const meTypeDescriptorMember& member)
		{
			DeepCopyContext fieldCtx = {};
			fieldCtx.srcData = member.data;
			fieldCtx.outputData = meSpan((u8*)ctx.outputData.data + member.offsetBytes, member.field.size);
			fieldCtx.allocator = ctx.allocator;
			fieldCtx.parentType = &typeDesc;
			meTypeDescriptorDeepCopy(member.field, fieldCtx);
			return true;
		});
}

meTypeDescriptor TD_UNSIGNED_INT = { .name = STRING_LIT("unsigned int"), .size = 4, .align = 4, .equalsFn = &meTypeDescriptorEquals<unsigned int> };
meTypeDescriptor TD_INT = { .name = STRING_LIT("int"), .size = 4, .align = 4, .equalsFn = &meTypeDescriptorEquals<int> };
meTypeDescriptor TD_UNSIGNED_SHORT = { .name = STRING_LIT("unsigned short"), .size = 2, .align = 2, .equalsFn = &meTypeDescriptorEquals<unsigned short> };
meTypeDescriptor TD_SHORT = { .name = STRING_LIT("short"), .size = 2, .align = 2, .equalsFn = &meTypeDescriptorEquals<short> };
meTypeDescriptor TD_UNSIGNED_LONG = { .name = STRING_LIT("unsigned long"), .size = 8, .align = 8, .equalsFn = &meTypeDescriptorEquals<unsigned long> };
meTypeDescriptor TD_LONG = { .name = STRING_LIT("long"), .size = 8, .align = 8, .equalsFn = &meTypeDescriptorEquals<long> };
meTypeDescriptor TD_LONG_LONG = { .name = STRING_LIT("long long"), .size = 8, .align = 8, .equalsFn = &meTypeDescriptorEquals<long long> };
meTypeDescriptor TD_UNSIGNED_LONG_LONG = { .name = STRING_LIT("unsigned long long"), .size = 8, .align = 8, .equalsFn = &meTypeDescriptorEquals<unsigned long long> };
meTypeDescriptor TD_FLOAT = { .name = STRING_LIT("float"), .size = 4, .align = 4, .equalsFn = &meTypeDescriptorEquals<float> };
meTypeDescriptor TD_DOUBLE = { .name = STRING_LIT("double"), .size = 8, .align = 8, .equalsFn = &meTypeDescriptorEquals<double> };
meTypeDescriptor TD_BOOL = { .name = STRING_LIT("bool"), .size = 1, .align = 1, .equalsFn = &meTypeDescriptorEquals<bool> };
meTypeDescriptor TD_CHAR = { .name = STRING_LIT("char"), .size = 1, .align = 1, .equalsFn = &meTypeDescriptorEquals<char> };
meTypeDescriptor TD_UNSIGNED_CHAR = {.name = STRING_LIT("unsigned char"), .size = 1, .align = 1, .equalsFn = &meTypeDescriptorEquals<unsigned char> };
meTypeDescriptor TD_WCHAR_T = { .name = STRING_LIT("wchar_t"), .size = 4, .align = 4, .equalsFn = &meTypeDescriptorEquals<wchar_t> };

// NOTE: We can reuse sizedBufferEquals because meSpan, StringView, and String follow a similar pattern internally
// where the first param is a data pointer and the second is the 64-bit size.
// equalsFn uses sizedBufferEquals for the same reason - we compare the referenced bytes instead of the raw struct.
meTypeDescriptor TD_SPAN = { .name = STRING_LIT("span"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(meSpan), .align = alignof(meSpan), .serializerFn = sizedBufferSerializer, .deserializerFn = sizedBufferDeserializer, .equalsFn = &sizedBufferEquals, .destroyFn = &sizedBufferDestroy, .deepCopyFn = &sizedBufferDeepCopy };
// NOTE: stringview is intentionally NOT serializable. If you want to serialize a string, use an owning String
meTypeDescriptor TD_STRINGVIEW = { .name = STRING_LIT("StringView"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(StringView), .align = alignof(StringView), .equalsFn = &sizedBufferEquals };
meTypeDescriptor TD_STRING = { .name = STRING_LIT("String"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(String), .align = alignof(String), .serializerFn = stringSerializer, .deserializerFn = stringDeserializer, .equalsFn = &sizedBufferEquals, .destroyFn = &stringDestroy, .deepCopyFn = &stringDeepCopy };

void DynArraySerializerToStringFn(
	const meTypeDescriptor& typeDescriptor,
	SerializeContext& ctx);

bool DynArrayDeserializerFromStringFn(
	const meTypeDescriptor& typeDescriptor,
	DeserializeContext& ctx);

static void DynArrayIterateContent(void* containerPtr, const meTypeDescriptor* fieldDesc,
                                   meTypeIterateElementFn visitor, void* userData)
{
    if (!fieldDesc->templatedTypes || fieldDesc->templatedTypes.size != 1) return;
    const meTypeDescriptor* elemType = meTypeDescriptorGetSingleTemplateArg(*fieldDesc);
    if (!elemType) return;
    DynArrayAny& arr = *(DynArrayAny*)containerPtr;
    if (!arr.data) return;
    u32 count = arr.header.size;
    for (u32 i = 0; i < count; i++)
    {
        u8* elemPtr = (u8*)arr.data + (i * elemType->size);
        visitor(elemPtr, elemType, (meContainerKey)i, userData);
    }
}

static void DynArrayPushElement(void* containerPtr, const meTypeDescriptor* /*fieldDesc*/, void* elemData)
{
    DynArrayAny& arr = *(DynArrayAny*)containerPtr;
    DynArrayPush(arr, (u8*)elemData, 1);
}

static void DynArrayRemoveElement(void* containerPtr, const meTypeDescriptor* /*fieldDesc*/, meContainerKey key)
{
    DynArrayAny& arr = *(DynArrayAny*)containerPtr;
    DynArrayPopAt(arr, (u32)key);
}

meTypeDescriptor TD_DYNARRAY = { .name = STRING_LIT("DynArray"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(DynArray<int>), .align = alignof(DynArray<int>),
#ifndef ME_CORE_ONLY
    .serializerFn = DynArraySerializerToStringFn, .deserializerFn = DynArrayDeserializerFromStringFn,
    .equalsFn = DynArrayEqualsFn,
	.destroyFn = DynArrayDestroyFn,
	.deepCopyFn = DynArrayDeepCopyFn,
#endif
    .iterateContentFn = DynArrayIterateContent,
    .pushElementFn    = DynArrayPushElement,
    .removeElementFn  = DynArrayRemoveElement,
};


