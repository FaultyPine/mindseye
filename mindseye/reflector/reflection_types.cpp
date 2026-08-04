
#include "reflection_types.h"
#include "core/me_string.h"
#include "core/me_math.h"
#include "core/me_log.h"

meTypeDescriptorRegistry& meTypeDescriptorGetRegistry()
{
	static meTypeDescriptorRegistry* registry = MENEW(GetDefaultAllocator(), meTypeDescriptorRegistry);
	return *registry;
}

void meTypeDescriptorRegister(u64 typeHash, meTypeDescriptor* typeDesc)
{
	ME_ASSERT(typeHash != 0);
	ME_ASSERT(typeDesc);

	meTypeDescriptorRegistry& registry = meTypeDescriptorGetRegistry();
	auto found = registry.find(typeHash);
	if (found != registry.end())
	{
		ME_ASSERT(found->second == typeDesc);
		return;
	}

	registry[typeHash] = typeDesc;
}

meTypeDescriptor* meTypeDescriptorFind(u64 typeHash)
{
	meTypeDescriptorRegistry& registry = meTypeDescriptorGetRegistry();
	auto found = registry.find(typeHash);
	return found != registry.end() ? found->second : nullptr;
}

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

	meTypeDescriptorDestroyFields(typeDesc, ctx);
}

void meTypeDescriptorDestroyFields(
	const meTypeDescriptor& typeDesc,
	DestroyContext& ctx)
{
	if (!ctx.data)
	{
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

	meTypeDescriptorDeepCopyFields(typeDesc, ctx);
}

void meTypeDescriptorDeepCopyFields(
	const meTypeDescriptor& typeDesc,
	DeepCopyContext& ctx)
{
	if (!ctx.srcData || !ctx.outputData.data)
	{
		return;
	}
	ME_ASSERT(ctx.outputData.size == typeDesc.size);

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

#ifndef ME_CORE_ONLY
#define ME_PRIMITIVE_SERDE .serializerFn = primitiveSerializer, .deserializerFn = primitiveDeserializer,
#define ME_SIZED_BUFFER_SERDE .serializerFn = sizedBufferSerializer, .deserializerFn = sizedBufferDeserializer,
#define ME_STRING_SERDE .serializerFn = stringSerializer, .deserializerFn = stringDeserializer,
#define ME_DISALLOWED_SERDE .serializerFn = serializationDisallowed, .deserializerFn = deserializationDisallowed,
#define ME_SPAN_DESTROY .destroyFn = &sizedBufferDestroy,
#define ME_STRING_DESTROY .destroyFn = &stringDestroy,
#define ME_SPAN_DEEP_COPY .deepCopyFn = &sizedBufferDeepCopy,
#define ME_STRING_DEEP_COPY .deepCopyFn = &stringDeepCopy,
#else
#define ME_PRIMITIVE_SERDE
#define ME_SIZED_BUFFER_SERDE
#define ME_STRING_SERDE
#define ME_DISALLOWED_SERDE
#define ME_SPAN_DESTROY
#define ME_STRING_DESTROY
#define ME_SPAN_DEEP_COPY
#define ME_STRING_DEEP_COPY
#endif

meTypeDescriptor TD_UNSIGNED_INT = { .name = STRING_LIT("unsigned int"), .size = 4, .align = 4, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<unsigned int> };
meTypeDescriptor TD_INT = { .name = STRING_LIT("int"), .size = 4, .align = 4, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<int> };
meTypeDescriptor TD_UNSIGNED_SHORT = { .name = STRING_LIT("unsigned short"), .size = 2, .align = 2, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<unsigned short> };
meTypeDescriptor TD_SHORT = { .name = STRING_LIT("short"), .size = 2, .align = 2, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<short> };
meTypeDescriptor TD_UNSIGNED_LONG = { .name = STRING_LIT("unsigned long"), .size = 8, .align = 8, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<unsigned long> };
meTypeDescriptor TD_LONG = { .name = STRING_LIT("long"), .size = 8, .align = 8, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<long> };
meTypeDescriptor TD_LONG_LONG = { .name = STRING_LIT("long long"), .size = 8, .align = 8, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<long long> };
meTypeDescriptor TD_UNSIGNED_LONG_LONG = { .name = STRING_LIT("unsigned long long"), .size = 8, .align = 8, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<unsigned long long> };
meTypeDescriptor TD_FLOAT = { .name = STRING_LIT("float"), .size = 4, .align = 4, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<float> };
meTypeDescriptor TD_DOUBLE = { .name = STRING_LIT("double"), .size = 8, .align = 8, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<double> };
meTypeDescriptor TD_BOOL = { .name = STRING_LIT("bool"), .size = 1, .align = 1, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<bool> };
meTypeDescriptor TD_CHAR = { .name = STRING_LIT("char"), .size = 1, .align = 1, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<char> };
meTypeDescriptor TD_UNSIGNED_CHAR = {.name = STRING_LIT("unsigned char"), .size = 1, .align = 1, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<unsigned char> };
meTypeDescriptor TD_WCHAR_T = { .name = STRING_LIT("wchar_t"), .size = 4, .align = 4, ME_PRIMITIVE_SERDE .equalsFn = &meTypeDescriptorEquals<wchar_t> };

ME_REGISTER_STATIC_TYPE_DESCRIPTOR(unsigned int, TD_UNSIGNED_INT);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(int, TD_INT);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(unsigned short, TD_UNSIGNED_SHORT);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(short, TD_SHORT);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(unsigned long, TD_UNSIGNED_LONG);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(long, TD_LONG);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(long long, TD_LONG_LONG);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(unsigned long long, TD_UNSIGNED_LONG_LONG);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(float, TD_FLOAT);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(double, TD_DOUBLE);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(bool, TD_BOOL);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(char, TD_CHAR);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(unsigned char, TD_UNSIGNED_CHAR);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(wchar_t, TD_WCHAR_T);

// NOTE: We can reuse sizedBufferEquals because meSpan, StringView, and String follow a similar pattern internally
// where the first param is a data pointer and the second is the 64-bit size.
// equalsFn uses sizedBufferEquals for the same reason - we compare the referenced bytes instead of the raw struct.
meTypeDescriptor TD_SPAN = { .name = STRING_LIT("span"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(meSpan), .align = alignof(meSpan), ME_SIZED_BUFFER_SERDE .equalsFn = &sizedBufferEquals, ME_SPAN_DESTROY ME_SPAN_DEEP_COPY };
// NOTE: StringView isn't allowed be serialized - it doesn't own the data. Use String instead.
meTypeDescriptor TD_STRINGVIEW = { .name = STRING_LIT("StringView"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(StringView), .align = alignof(StringView), ME_DISALLOWED_SERDE .equalsFn = &sizedBufferEquals };
meTypeDescriptor TD_STRING = { .name = STRING_LIT("String"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(String), .align = alignof(String), ME_STRING_SERDE .equalsFn = &sizedBufferEquals, ME_STRING_DESTROY ME_STRING_DEEP_COPY };

ME_REGISTER_STATIC_TYPE_DESCRIPTOR(meSpan, TD_SPAN);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(StringView, TD_STRINGVIEW);
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(String, TD_STRING);

#undef ME_PRIMITIVE_SERDE
#undef ME_SIZED_BUFFER_SERDE
#undef ME_STRING_SERDE
#undef ME_DISALLOWED_SERDE
#undef ME_SPAN_DESTROY
#undef ME_STRING_DESTROY
#undef ME_SPAN_DEEP_COPY
#undef ME_STRING_DEEP_COPY

bool DynArraySerializerToStringFn(
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

ME_REGISTER_STATIC_TYPE_DESCRIPTOR(DynArray<int>, TD_DYNARRAY);
