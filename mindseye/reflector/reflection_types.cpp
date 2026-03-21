
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
{
	meSpan fieldData = ctx.data;
	meAllocator* allocator = ctx.allocator;
	meSpan dereferencedData = *(meSpan*)fieldData.data;
	Allocation mem = MEALLOC(allocator, dereferencedData.size);
	BufferCopy(mem, dereferencedData);
	ctx.outputData = mem;
}
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

// NOTE: We can reuse the sizedbufferserializer ONLY because meSpan, StringView, and String follow a similar pattern internally
// where the first param is a data pointer and the second is the 64bit size.
meTypeDescriptor TD_SPAN = { .name = STRING_LIT("span"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(meSpan), .align = alignof(meSpan), .serializerFn = sizedBufferSerializer, .deserializerFn = sizedBufferDeserializer };
meTypeDescriptor TD_STRINGVIEW = { .name = STRING_LIT("StringView"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(StringView), .align = alignof(StringView), .serializerFn = sizedBufferSerializer, .deserializerFn = sizedBufferDeserializer };
meTypeDescriptor TD_STRING = { .name = STRING_LIT("String"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(String), .align = alignof(String), .serializerFn = sizedBufferSerializer, .deserializerFn = stringDeserializer };
meTypeDescriptor TD_DYNARRAY = { .name = STRING_LIT("DynArray"), .flags = meTypeDescriptorFlag_ExternalPtr, .size = sizeof(DynArray<int>), .align = alignof(DynArray<int>), 
#ifndef ME_CORE_ONLY
    .serializerFn = DynArraySerializerToStringFn, .deserializerFn = DynArrayDeserializerFromStringFn 
#endif
};


