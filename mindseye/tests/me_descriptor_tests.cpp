#include "tests/me_descriptor_tests.h"

#include "core/me_log.h"
#include "core/me_serialize.h"
#include "core/me_filesystem.h"
#include "generatedtypes/me_descriptor_tests.generated.h"
#include "generatedtypes/me_filesystem.generated.h"

#include <stddef.h>

struct meDescriptorDispatchValue
{
    s32 value = 0;
};

struct meDescriptorDispatchWrapper
{
    meDescriptorDispatchValue field = {};
};

static bool DescriptorDispatchBaseSerializer(const meTypeDescriptor&, SerializeContext& ctx);
static bool DescriptorDispatchAliasSerializer(const meTypeDescriptor&, SerializeContext& ctx);
static bool DescriptorDispatchBaseDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
static bool DescriptorDispatchAliasDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
static bool DescriptorDispatchBaseEquals(const meTypeDescriptor&, const void* a, const void* b);
static bool DescriptorDispatchAliasEquals(const meTypeDescriptor&, const void* a, const void* b);
static void DescriptorDispatchBaseDestroy(const meTypeDescriptor&, DestroyContext& ctx);
static void DescriptorDispatchAliasDestroy(const meTypeDescriptor&, DestroyContext& ctx);
static void DescriptorDispatchBaseDeepCopy(const meTypeDescriptor&, DeepCopyContext& ctx);
static void DescriptorDispatchAliasDeepCopy(const meTypeDescriptor&, DeepCopyContext& ctx);

static meTypeDescriptor TD_DESCRIPTOR_DISPATCH_BASE = {
    .name = STRING_LIT("meDescriptorDispatchValue"),
    .size = sizeof(meDescriptorDispatchValue),
    .align = alignof(meDescriptorDispatchValue),
    .serializerFn = DescriptorDispatchBaseSerializer,
    .deserializerFn = DescriptorDispatchBaseDeserializer,
    .equalsFn = DescriptorDispatchBaseEquals,
    .destroyFn = DescriptorDispatchBaseDestroy,
    .deepCopyFn = DescriptorDispatchBaseDeepCopy,
};
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(meDescriptorDispatchValue, TD_DESCRIPTOR_DISPATCH_BASE);

static meTypeDescriptor g_descriptorDispatchWrapperFields[] = {
    {
        .name = STRING_LIT("field"),
        .size = sizeof(meDescriptorDispatchValue),
        .align = alignof(meDescriptorDispatchValue),
        .offsetBits = 0,
        .thisType = &TD_DESCRIPTOR_DISPATCH_BASE,
        .serializerFn = DescriptorDispatchAliasSerializer,
        .deserializerFn = DescriptorDispatchAliasDeserializer,
        .equalsFn = DescriptorDispatchAliasEquals,
        .destroyFn = DescriptorDispatchAliasDestroy,
        .deepCopyFn = DescriptorDispatchAliasDeepCopy,
    },
};

static meTypeDescriptor TD_DESCRIPTOR_DISPATCH_WRAPPER = {
    .name = STRING_LIT("meDescriptorDispatchWrapper"),
    .fields = { g_descriptorDispatchWrapperFields },
    .size = sizeof(meDescriptorDispatchWrapper),
    .align = alignof(meDescriptorDispatchWrapper),
};
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(meDescriptorDispatchWrapper, TD_DESCRIPTOR_DISPATCH_WRAPPER);

struct DescriptorDispatchStats
{
    u32 baseSerializer = 0;
    u32 aliasSerializer = 0;
    u32 baseDeserializer = 0;
    u32 aliasDeserializer = 0;
    u32 baseEquals = 0;
    u32 aliasEquals = 0;
    u32 baseDestroy = 0;
    u32 aliasDestroy = 0;
    u32 baseDeepCopy = 0;
    u32 aliasDeepCopy = 0;
};

static DescriptorDispatchStats g_descriptorDispatchStats = {};
static u32 g_descriptorLifecycleMemberDestroyCalls = 0;

struct meSerializationFixture
{
    meSerializedHeader header = {};
    s8 i8 = 0;
    u8 u8v = 0;
    s16 i16 = 0;
    u16 u16v = 0;
    s32 i32 = 0;
    u32 u32v = 0;
    s64 i64 = 0;
    u64 u64v = 0;
    f32 f32v = 0.0f;
    f64 f64v = 0.0;
    bool truthy = false;
    bool falsy = true;
    String text = {};
    meSpan bytes = {};
    s32 fixed[4] = {};
};

static meTypeDescriptor g_serializationFixtureFields[] = {
    { .name = STRING_LIT("header"), .size = sizeof(meSerializedHeader), .align = alignof(meSerializedHeader), .offsetBits = (s32)(offsetof(meSerializationFixture, header) * 8), .thisType = &TD_MESERIALIZEDHEADER, },
    { .name = STRING_LIT("i8"), .size = sizeof(s8), .align = alignof(s8), .offsetBits = (s32)(offsetof(meSerializationFixture, i8) * 8), .thisType = &TD_CHAR, },
    { .name = STRING_LIT("u8v"), .size = sizeof(u8), .align = alignof(u8), .offsetBits = (s32)(offsetof(meSerializationFixture, u8v) * 8), .thisType = &TD_UNSIGNED_CHAR, },
    { .name = STRING_LIT("i16"), .size = sizeof(s16), .align = alignof(s16), .offsetBits = (s32)(offsetof(meSerializationFixture, i16) * 8), .thisType = &TD_SHORT, },
    { .name = STRING_LIT("u16v"), .size = sizeof(u16), .align = alignof(u16), .offsetBits = (s32)(offsetof(meSerializationFixture, u16v) * 8), .thisType = &TD_UNSIGNED_SHORT, },
    { .name = STRING_LIT("i32"), .size = sizeof(s32), .align = alignof(s32), .offsetBits = (s32)(offsetof(meSerializationFixture, i32) * 8), .thisType = &TD_INT, },
    { .name = STRING_LIT("u32v"), .size = sizeof(u32), .align = alignof(u32), .offsetBits = (s32)(offsetof(meSerializationFixture, u32v) * 8), .thisType = &TD_UNSIGNED_INT, },
    { .name = STRING_LIT("i64"), .size = sizeof(s64), .align = alignof(s64), .offsetBits = (s32)(offsetof(meSerializationFixture, i64) * 8), .thisType = &TD_LONG_LONG, },
    { .name = STRING_LIT("u64v"), .size = sizeof(u64), .align = alignof(u64), .offsetBits = (s32)(offsetof(meSerializationFixture, u64v) * 8), .thisType = &TD_UNSIGNED_LONG_LONG, },
    { .name = STRING_LIT("f32v"), .size = sizeof(f32), .align = alignof(f32), .offsetBits = (s32)(offsetof(meSerializationFixture, f32v) * 8), .thisType = &TD_FLOAT, },
    { .name = STRING_LIT("f64v"), .size = sizeof(f64), .align = alignof(f64), .offsetBits = (s32)(offsetof(meSerializationFixture, f64v) * 8), .thisType = &TD_DOUBLE, },
    { .name = STRING_LIT("truthy"), .size = sizeof(bool), .align = alignof(bool), .offsetBits = (s32)(offsetof(meSerializationFixture, truthy) * 8), .thisType = &TD_BOOL, },
    { .name = STRING_LIT("falsy"), .size = sizeof(bool), .align = alignof(bool), .offsetBits = (s32)(offsetof(meSerializationFixture, falsy) * 8), .thisType = &TD_BOOL, },
    { .name = STRING_LIT("text"), .size = sizeof(String), .align = alignof(String), .offsetBits = (s32)(offsetof(meSerializationFixture, text) * 8), .thisType = &TD_STRING, },
    { .name = STRING_LIT("bytes"), .size = sizeof(meSpan), .align = alignof(meSpan), .offsetBits = (s32)(offsetof(meSerializationFixture, bytes) * 8), .thisType = &TD_SPAN, },
    { .name = STRING_LIT("fixed"), .flags = NTH_BIT(meTypeDescriptorFlag_ConstantArray), .size = sizeof(((meSerializationFixture*)0)->fixed), .align = alignof(s32), .offsetBits = (s32)(offsetof(meSerializationFixture, fixed) * 8), .thisType = &TD_INT, },
};

static meTypeDescriptor TD_SERIALIZATION_FIXTURE = {
    .name = STRING_LIT("meSerializationFixture"),
    .fields = { g_serializationFixtureFields },
    .version = 7,
    .size = sizeof(meSerializationFixture),
    .align = alignof(meSerializationFixture),
    .setToDefaultsFn = &meTypeDescriptorSetToDefaults<meSerializationFixture>,
};
ME_REGISTER_STATIC_TYPE_DESCRIPTOR(meSerializationFixture, TD_SERIALIZATION_FIXTURE);

static void DescriptorDispatchResetStats()
{
    g_descriptorDispatchStats = {};
}

void meDescriptorLifecycleWithDestroy::Destroy()
{
    g_descriptorLifecycleMemberDestroyCalls++;
    name.~String();
    new (&name) String();

    for (DynArray_Foreach(destroyAliases, i))
    {
        destroyAliases[i].~String();
    }
    DynArrayDestroy(destroyAliases);
}

static bool DescriptorDispatchBaseSerializer(const meTypeDescriptor&, SerializeContext& ctx)
{
    g_descriptorDispatchStats.baseSerializer++;
    const meDescriptorDispatchValue& value = *(const meDescriptorDispatchValue*)ctx.sourceData.data;
    s32 serializedValue = value.value + 2000;
    ctx.sourceData = meSpan(&serializedValue, sizeof(serializedValue));
    return primitiveSerializer(TD_INT, ctx);
}

static bool DescriptorDispatchAliasSerializer(const meTypeDescriptor&, SerializeContext& ctx)
{
    g_descriptorDispatchStats.aliasSerializer++;
    const meDescriptorDispatchValue& value = *(const meDescriptorDispatchValue*)ctx.sourceData.data;
    s32 serializedValue = value.value + 1000;
    ctx.sourceData = meSpan(&serializedValue, sizeof(serializedValue));
    return primitiveSerializer(TD_INT, ctx);
}

static bool DescriptorDispatchBaseDeserializer(const meTypeDescriptor&, DeserializeContext& ctx)
{
    g_descriptorDispatchStats.baseDeserializer++;
    s32 serializedValue = 0;
    meSpan originalOutput = ctx.outputData;
    ctx.outputData = meSpan(&serializedValue, sizeof(serializedValue));
    if (!primitiveDeserializer(TD_INT, ctx))
    {
        ctx.outputData = originalOutput;
        return false;
    }
    ctx.outputData = originalOutput;
    ((meDescriptorDispatchValue*)ctx.outputData.data)->value = serializedValue - 2000;
    return true;
}

static bool DescriptorDispatchAliasDeserializer(const meTypeDescriptor&, DeserializeContext& ctx)
{
    g_descriptorDispatchStats.aliasDeserializer++;
    s32 serializedValue = 0;
    meSpan originalOutput = ctx.outputData;
    ctx.outputData = meSpan(&serializedValue, sizeof(serializedValue));
    if (!primitiveDeserializer(TD_INT, ctx))
    {
        ctx.outputData = originalOutput;
        return false;
    }
    ctx.outputData = originalOutput;
    ((meDescriptorDispatchValue*)ctx.outputData.data)->value = serializedValue - 1000;
    return true;
}

static bool DescriptorDispatchBaseEquals(const meTypeDescriptor& td, const void* a, const void* b)
{
    g_descriptorDispatchStats.baseEquals++;
    ME_ASSERT(&td == &g_descriptorDispatchWrapperFields[0]);
    const meDescriptorDispatchValue& lhs = *(const meDescriptorDispatchValue*)a;
    const meDescriptorDispatchValue& rhs = *(const meDescriptorDispatchValue*)b;
    return (lhs.value % 10) == (rhs.value % 10);
}

static bool DescriptorDispatchAliasEquals(const meTypeDescriptor&, const void*, const void*)
{
    g_descriptorDispatchStats.aliasEquals++;
    return true;
}

static void DescriptorDispatchBaseDestroy(const meTypeDescriptor&, DestroyContext& ctx)
{
    g_descriptorDispatchStats.baseDestroy++;
    ((meDescriptorDispatchValue*)ctx.data)->value = -2000;
}

static void DescriptorDispatchAliasDestroy(const meTypeDescriptor&, DestroyContext& ctx)
{
    g_descriptorDispatchStats.aliasDestroy++;
    ((meDescriptorDispatchValue*)ctx.data)->value = -1000;
}

static void DescriptorDispatchBaseDeepCopy(const meTypeDescriptor&, DeepCopyContext& ctx)
{
    g_descriptorDispatchStats.baseDeepCopy++;
    const meDescriptorDispatchValue& src = *(const meDescriptorDispatchValue*)ctx.srcData;
    ((meDescriptorDispatchValue*)ctx.outputData.data)->value = src.value + 2000;
}

static void DescriptorDispatchAliasDeepCopy(const meTypeDescriptor&, DeepCopyContext& ctx)
{
    g_descriptorDispatchStats.aliasDeepCopy++;
    const meDescriptorDispatchValue& src = *(const meDescriptorDispatchValue*)ctx.srcData;
    ((meDescriptorDispatchValue*)ctx.outputData.data)->value = src.value + 1000;
}

static meDescriptorTestAsset MakeDescriptorTestAsset(meAllocator* allocator)
{
    meDescriptorTestAsset asset = {};
    asset.header.assetHeader = MAID(12345, MAEntity);
    asset.displayName = String(STRING_LIT("descriptor fixture"), allocator);
    asset.health = 100;
    asset.speed = 12.5f;
    asset.samples[0] = 7;
    asset.samples[1] = -4;
    asset.samples[2] = 99;
    asset.children = DynArrayCreate<meDescriptorTestChild>(allocator);

    meDescriptorTestChild first = { .id = 1, .weight = 0.25f, .enabled = true };
    meDescriptorTestChild second = { .id = 2, .weight = 1.5f, .enabled = false };
    DynArrayPush(asset.children, first);
    DynArrayPush(asset.children, second);

    return asset;
}

static void AddLifecycleAlias(
    DynArray<String>& aliases,
    meAllocator* allocator,
    StringView alias)
{
    DynArrayHeader* header = GetHeaderPointer(aliases);
    ME_ASSERT(header->size < header->capacity);
    new (&aliases.data[header->size]) String(alias, allocator);
    header->size++;
}

static meDescriptorLifecycleContainer MakeLifecycleContainer(meAllocator* allocator)
{
    meDescriptorLifecycleContainer data = {};
    data.name = String(STRING_LIT("lifecycle root"), allocator);
    data.aliases = DynArrayCreate<String>(allocator);
    AddLifecycleAlias(data.aliases, allocator, STRING_LIT("first alias"));
    AddLifecycleAlias(data.aliases, allocator, STRING_LIT("second alias"));
    return data;
}

static meDescriptorLifecycleWithDestroy MakeLifecycleWithDestroy(meAllocator* allocator)
{
    meDescriptorLifecycleWithDestroy data = {};
    data.name = String(STRING_LIT("member destroy root"), allocator);
    data.destroyAliases = DynArrayCreate<String>(allocator);
    AddLifecycleAlias(data.destroyAliases, allocator, STRING_LIT("member first"));
    AddLifecycleAlias(data.destroyAliases, allocator, STRING_LIT("member second"));
    return data;
}

static void DestroyDescriptorTestAsset(meDescriptorTestAsset& asset)
{
    DynArrayDestroy(asset.children);
}

enum class SerializationFixtureByteMode
{
    Utf8,
    Arbitrary,
};

static void SetSerializationFixtureBytes(
    meSerializationFixture& fixture,
    meAllocator* allocator,
    const u8* bytes,
    u32 byteCount)
{
    fixture.bytes = MEALLOC(allocator, byteCount);
    ME_MEMCPY(fixture.bytes.data, (void*)bytes, byteCount);
}

static SerializationFixtureByteMode ByteModeForSerializationMode(meSerializationMode mode)
{
    return mode == meSerializationMode_Text
        ? SerializationFixtureByteMode::Utf8
        : SerializationFixtureByteMode::Arbitrary;
}

static void InitSerializationFixture(
    meSerializationFixture& fixture,
    meAllocator* allocator,
    SerializationFixtureByteMode byteMode = SerializationFixtureByteMode::Arbitrary)
{
    fixture.i8 = -120;
    fixture.u8v = 250;
    fixture.i16 = -32000;
    fixture.u16v = 65000;
    fixture.i32 = -123456789;
    fixture.u32v = 4000000000U;
    fixture.i64 = -123456789012345678LL;
    fixture.u64v = 12345678901234567890ULL;
    fixture.f32v = -12.5f;
    fixture.f64v = 12345.6789;
    fixture.truthy = true;
    fixture.falsy = false;
    fixture.text = String(STRING_LIT("quotes \" slash \\ newline \n unicode-ish ascii"), allocator);

    if (byteMode == SerializationFixtureByteMode::Utf8)
    {
        constexpr u8 bytes[] = { 'u', 't', 'f', '8', 0, 1, 2, 127 };
        SetSerializationFixtureBytes(fixture, allocator, bytes, sizeof(bytes));
    }
    else
    {
        constexpr u8 bytes[] = { 0, 1, 2, 127, 128, 255 };
        SetSerializationFixtureBytes(fixture, allocator, bytes, sizeof(bytes));
    }

    fixture.fixed[0] = -7;
    fixture.fixed[1] = 0;
    fixture.fixed[2] = 42;
    fixture.fixed[3] = 9001;
}

static void DestroySerializationFixture(meSerializationFixture& fixture, meAllocator* allocator)
{
    if (fixture.bytes)
    {
        MEFREE(allocator, fixture.bytes.data);
        fixture.bytes = {};
    }
}

static meSerializeResult SerializeForTest(
    const meTypeDescriptor& typeDesc,
    meSerializationMode mode,
    void* sourceData,
    meAllocator* allocator,
    meOwningSpan& outData)
{
    SerializeContext ctx = {};
    ctx.mode = mode;
    ctx.typeDesc = &typeDesc;
    ctx.allocator = allocator;
    ctx.sourceData = meSpan(sourceData, typeDesc.size);
    meSerializeResult result = SerializeBlocking(ctx);
    outData = ctx.serializedData;
    return result;
}

static meSerializeResult DeserializeForTest(
    const meTypeDescriptor& typeDesc,
    meSerializationMode mode,
    meSpan input,
    void* outputData,
    meAllocator* allocator)
{
    meSerializeResult result = {};
    DeserializeContext ctx = {};
    ctx.mode = mode;
    ctx.typeDesc = &typeDesc;
    ctx.externalDataAllocator = allocator;
    ctx.sourceData = input;
    ctx.outputData = meSpan(outputData, typeDesc.size);
    ctx.outResult = &result;
    DeserializeBlocking(ctx);
    return result;
}

static void DescriptorTestSerializationFixtureRoundTrip(meAllocator* allocator, meSerializationMode mode)
{
    meSerializationFixture original = {};
    meSerializationFixture restored = {};
    InitSerializationFixture(original, allocator, ByteModeForSerializationMode(mode));

    meOwningSpan serialized = {};
    meSerializeResult serializeResult = SerializeForTest(TD_SERIALIZATION_FIXTURE, mode, &original, allocator, serialized);
    ME_ASSERT(serializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(serialized);

    if (mode == meSerializationMode_Binary)
    {
        meSerializedHeader header = {};
        ME_ASSERT(meSerializeTryReadBinaryHeader(serialized, &header));
        ME_ASSERT(header.typeVersion == TD_SERIALIZATION_FIXTURE.version);
        ME_ASSERT(header.payloadSize == 0);
    }
    else
    {
        StringView text = StringView::FromSpan(serialized);
        ME_ASSERT(!meSerializeTryReadBinaryHeader(serialized));
        ME_ASSERT(FindInString(text, STRING_LIT("\"header\"")) >= 0);
        ME_ASSERT(FindInString(text, STRING_LIT("\"payload\"")) == -1);
        ME_ASSERT(FindInString(text, STRING_LIT("quotes")) >= 0);
    }

    meSerializeResult deserializeResult = DeserializeForTest(
        TD_SERIALIZATION_FIXTURE,
        mode,
        serialized,
        &restored,
        allocator);
    ME_ASSERT(deserializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(meFieldsEqual(TD_SERIALIZATION_FIXTURE, &original, &restored));
    ME_ASSERT(restored.text.data != original.text.data);
    ME_ASSERT(restored.bytes.data != original.bytes.data);

    MEFREE(allocator, serialized.data);
    DestroySerializationFixture(original, allocator);
    DestroySerializationFixture(restored, allocator);
}

static void DescriptorTestTextInvalidUtf8BytesFail(meAllocator* allocator)
{
    meSerializationFixture original = {};
    InitSerializationFixture(original, allocator, SerializationFixtureByteMode::Arbitrary);

    meOwningSpan serialized = {};
    meSerializeResult serializeResult = SerializeForTest(
        TD_SERIALIZATION_FIXTURE,
        meSerializationMode_Text,
        &original,
        allocator,
        serialized);
    ME_ASSERT(serializeResult == meSerializeResult::SER_FAILURE);
    ME_ASSERT(!serialized);

    DestroySerializationFixture(original, allocator);
}

static void DescriptorTestEmptyValuesRoundTrip(meAllocator* allocator, meSerializationMode mode)
{
    meDescriptorTestAsset original = {};
    meDescriptorTestAsset restored = {};
    original.header.assetHeader = MAID(2222, MAEntity);
    original.displayName = String(STRING_LIT(""), allocator);
    original.health = 0;
    original.speed = 0.0f;
    original.children = DynArrayCreate<meDescriptorTestChild>(allocator);

    meOwningSpan serialized = {};
    meSerializeResult serializeResult = SerializeForTest(TD_MEDESCRIPTORTESTASSET, mode, &original, allocator, serialized);
    ME_ASSERT(serializeResult == meSerializeResult::SER_SUCCESS);

    meSerializeResult deserializeResult = DeserializeForTest(
        TD_MEDESCRIPTORTESTASSET,
        mode,
        serialized,
        &restored,
        allocator);
    ME_ASSERT(deserializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));
    ME_ASSERT(DynArrayGetSize(restored.children) == 0);

    MEFREE(allocator, serialized.data);
    DestroyDescriptorTestAsset(original);
    DestroyDescriptorTestAsset(restored);
}

static void DescriptorTestWrongModeFails(meAllocator* allocator)
{
    meSerializationFixture original = {};
    meSerializationFixture restored = {};
    InitSerializationFixture(original, allocator, SerializationFixtureByteMode::Utf8);

    meOwningSpan textSerialized = {};
    ME_ASSERT(SerializeForTest(TD_SERIALIZATION_FIXTURE, meSerializationMode_Text, &original, allocator, textSerialized) == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(DeserializeForTest(TD_SERIALIZATION_FIXTURE, meSerializationMode_Binary, textSerialized, &restored, allocator) == meSerializeResult::SER_FAILURE);

    meOwningSpan binarySerialized = {};
    ME_ASSERT(SerializeForTest(TD_SERIALIZATION_FIXTURE, meSerializationMode_Binary, &original, allocator, binarySerialized) == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(DeserializeForTest(TD_SERIALIZATION_FIXTURE, meSerializationMode_Text, binarySerialized, &restored, allocator) == meSerializeResult::SER_FAILURE);

    MEFREE(allocator, textSerialized.data);
    MEFREE(allocator, binarySerialized.data);
    DestroySerializationFixture(original, allocator);
    DestroySerializationFixture(restored, allocator);
}

static void DescriptorTestTruncatedDataFails(meAllocator* allocator, meSerializationMode mode)
{
    meSerializationFixture original = {};
    meSerializationFixture restored = {};
    InitSerializationFixture(original, allocator, ByteModeForSerializationMode(mode));

    meOwningSpan serialized = {};
    ME_ASSERT(SerializeForTest(TD_SERIALIZATION_FIXTURE, mode, &original, allocator, serialized) == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(serialized.size > 1);

    meSpan truncated(serialized.data, serialized.size - 1);
    ME_ASSERT(DeserializeForTest(TD_SERIALIZATION_FIXTURE, mode, truncated, &restored, allocator) == meSerializeResult::SER_FAILURE);

    MEFREE(allocator, serialized.data);
    DestroySerializationFixture(original, allocator);
    DestroySerializationFixture(restored, allocator);
}

static void DescriptorTestVersionMismatchFails(meAllocator* allocator, meSerializationMode mode)
{
    meSerializationFixture original = {};
    meSerializationFixture restored = {};
    InitSerializationFixture(original, allocator, ByteModeForSerializationMode(mode));

    meOwningSpan serialized = {};
    s32 originalVersion = TD_SERIALIZATION_FIXTURE.version;
    ME_ASSERT(SerializeForTest(TD_SERIALIZATION_FIXTURE, mode, &original, allocator, serialized) == meSerializeResult::SER_SUCCESS);

    TD_SERIALIZATION_FIXTURE.version = originalVersion + 1;
    meSerializeResult deserializeResult = DeserializeForTest(TD_SERIALIZATION_FIXTURE, mode, serialized, &restored, allocator);
    TD_SERIALIZATION_FIXTURE.version = originalVersion;
    ME_ASSERT(deserializeResult == meSerializeResult::SER_VERSION_MISMATCH);

    MEFREE(allocator, serialized.data);
    DestroySerializationFixture(original, allocator);
    DestroySerializationFixture(restored, allocator);
}

static void DescriptorTestTypeMismatchFails(meAllocator* allocator, meSerializationMode mode)
{
    meSerializationFixture original = {};
    meDescriptorTestChild wrongOutput = {};
    InitSerializationFixture(original, allocator, ByteModeForSerializationMode(mode));

    meOwningSpan serialized = {};
    ME_ASSERT(SerializeForTest(TD_SERIALIZATION_FIXTURE, mode, &original, allocator, serialized) == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(DeserializeForTest(TD_MEDESCRIPTORTESTCHILD, mode, serialized, &wrongOutput, allocator) == meSerializeResult::SER_FAILURE);

    MEFREE(allocator, serialized.data);
    DestroySerializationFixture(original, allocator);
}

static const meTypeDescriptor* FindDescriptorField(const meTypeDescriptor& typeDesc, StringView name)
{
    const meTypeDescriptor* result = nullptr;
    for (u32 i = 0; i < typeDesc.fields.size; i++)
    {
        const meTypeDescriptor& member = typeDesc.fields[i];
        if (StringCompare(member.name, name))
        {
            result = &member;
            break;
        }
    }
    ME_ASSERT(result);
    return result;
}

static const meTypeDescriptor* FindDescriptorTestField(StringView name)
{
    return FindDescriptorField(TD_MEDESCRIPTORTESTASSET, name);
}

static void DescriptorTestGeneratedLifecycleHookPresence()
{
    ME_ASSERT(TD_MEDESCRIPTORTESTCHILD.setToDefaultsFn);
    ME_ASSERT(TD_MEDESCRIPTORTESTCHILD.destroyFn);
    ME_ASSERT(TD_MEDESCRIPTORTESTCHILD.deepCopyFn);
    ME_ASSERT(TD_MEDESCRIPTORTESTASSET.setToDefaultsFn);
    ME_ASSERT(TD_MEDESCRIPTORTESTASSET.destroyFn);
    ME_ASSERT(TD_MEDESCRIPTORTESTASSET.deepCopyFn);
    ME_ASSERT(TD_MEDESCRIPTORLIFECYCLECONTAINER.setToDefaultsFn);
    ME_ASSERT(TD_MEDESCRIPTORLIFECYCLECONTAINER.destroyFn);
    ME_ASSERT(TD_MEDESCRIPTORLIFECYCLECONTAINER.deepCopyFn);
    ME_ASSERT(TD_MEDESCRIPTORLIFECYCLEWITHDESTROY.setToDefaultsFn);
    ME_ASSERT(TD_MEDESCRIPTORLIFECYCLEWITHDESTROY.destroyFn);
    ME_ASSERT(TD_MEDESCRIPTORLIFECYCLEWITHDESTROY.deepCopyFn);

    ME_ASSERT(TD_MEFSPATH.serializerFn == stringSerializer);
    ME_ASSERT(TD_MEFSPATH.deserializerFn == stringDeserializer);
    ME_ASSERT(TD_MEFSPATH.equalsFn == sizedBufferEquals);
    ME_ASSERT(TD_MEFSPATH.destroyFn == stringDestroy);
    ME_ASSERT(TD_MEFSPATH.deepCopyFn == stringDeepCopy);
}

static void DescriptorTestTypeDescriptorRegistry()
{
    ME_ASSERT(meTypeDescriptorFind<int>() == &TD_INT);
    ME_ASSERT(meTypeDescriptorFind<String>() == &TD_STRING);
    ME_ASSERT(meTypeDescriptorFind<meDescriptorTestChild>() == &TD_MEDESCRIPTORTESTCHILD);
    ME_ASSERT(meTypeDescriptorFind<meDescriptorTestAsset>() == &TD_MEDESCRIPTORTESTASSET);
    ME_ASSERT(meTypeDescriptorFind<meDescriptorDispatchValue>() == &TD_DESCRIPTOR_DISPATCH_BASE);
    ME_ASSERT(meTypeDescriptorFind<meDescriptorDispatchWrapper>() == &TD_DESCRIPTOR_DISPATCH_WRAPPER);
    ME_ASSERT(meTypeDescriptorFind<meSerializationFixture>() == &TD_SERIALIZATION_FIXTURE);
    ME_ASSERT(meTypeDescriptorFind<meFsPath>() == &TD_MEFSPATH);
}

static void DescriptorTestSetToDefaults()
{
    alignas(meDescriptorTestAsset) u8 backing[sizeof(meDescriptorTestAsset)];
    ME_MEMSET(backing, 0xCD, sizeof(backing));

    TD_MEDESCRIPTORTESTASSET.setToDefaultsFn(backing);
    meDescriptorTestAsset& asset = *(meDescriptorTestAsset*)backing;

    ME_ASSERT(!asset.header.assetHeader);
    ME_ASSERT(!asset.displayName);
    ME_ASSERT(asset.health == 0);
    ME_ASSERT(asset.speed == 0.0f);
    ME_ASSERT(asset.samples[0] == 0);
    ME_ASSERT(asset.samples[1] == 0);
    ME_ASSERT(asset.samples[2] == 0);
    ME_ASSERT(!asset.children);

    asset.~meDescriptorTestAsset();

    alignas(meDescriptorLifecycleContainer) u8 lifecycleBacking[sizeof(meDescriptorLifecycleContainer)];
    ME_MEMSET(lifecycleBacking, 0xCD, sizeof(lifecycleBacking));

    TD_MEDESCRIPTORLIFECYCLECONTAINER.setToDefaultsFn(lifecycleBacking);
    meDescriptorLifecycleContainer& lifecycle = *(meDescriptorLifecycleContainer*)lifecycleBacking;

    ME_ASSERT(!lifecycle.name);
    ME_ASSERT(!lifecycle.aliases);

    lifecycle.~meDescriptorLifecycleContainer();
}

static void DescriptorTestSerializeDeserializeEquals(meAllocator* allocator)
{
    meDescriptorTestAsset original = MakeDescriptorTestAsset(allocator);
    meDescriptorTestAsset restored = {};

    SerializeContext serializeCtx = {};
    serializeCtx.mode = meSerializationMode_Text;
    serializeCtx.typeDesc = &TD_MEDESCRIPTORTESTASSET;
    serializeCtx.allocator = allocator;
    serializeCtx.sourceData = meSpan(&original, sizeof(original));
    meSerializeResult serializeResult = SerializeBlocking(serializeCtx);
    ME_ASSERT(serializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(serializeCtx.serializedData);

    meSerializeResult deserializeResult = {};
    DeserializeContext deserializeCtx = {};
    deserializeCtx.mode = meSerializationMode_Text;
    deserializeCtx.typeDesc = &TD_MEDESCRIPTORTESTASSET;
    deserializeCtx.externalDataAllocator = allocator;
    deserializeCtx.sourceData = serializeCtx.serializedData;
    deserializeCtx.outputData = meSpan(&restored, sizeof(restored));
    deserializeCtx.outResult = &deserializeResult;
    DeserializeBlocking(deserializeCtx);
    ME_ASSERT(deserializeResult == meSerializeResult::SER_SUCCESS);

    ME_ASSERT(meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));
    ME_ASSERT(!TD_MEDESCRIPTORTESTASSET.equalsFn);

    meSpan headerSpan = meSerializeTryGetAssetHeader(
        TD_MEDESCRIPTORTESTASSET,
        meSpan(&restored, sizeof(restored)));
    ME_ASSERT(headerSpan.data);
    ME_ASSERT(*(MAID*)headerSpan.data == original.header.assetHeader);

    restored.samples[1]++;
    ME_ASSERT(!meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));
    restored.samples[1] = original.samples[1];

    restored.children[1].weight += 1.0f;
    ME_ASSERT(!meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));

    MEFREE(allocator, serializeCtx.serializedData.data);
    DestroyDescriptorTestAsset(original);
    DestroyDescriptorTestAsset(restored);
}

static void DescriptorTestBinarySerializeDeserializeEquals(meAllocator* allocator)
{
    meDescriptorTestAsset original = MakeDescriptorTestAsset(allocator);
    meDescriptorTestAsset restored = {};

    SerializeContext serializeCtx = {};
    serializeCtx.mode = meSerializationMode_Binary;
    serializeCtx.typeDesc = &TD_MEDESCRIPTORTESTASSET;
    serializeCtx.allocator = allocator;
    serializeCtx.sourceData = meSpan(&original, sizeof(original));
    meSerializeResult serializeResult = SerializeBlocking(serializeCtx);
    ME_ASSERT(serializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(serializeCtx.serializedData);

    meSerializedHeader header = {};
    ME_ASSERT(meSerializeTryReadBinaryHeader(serializeCtx.serializedData, &header));
    ME_ASSERT(header.assetHeader == original.header.assetHeader);
    ME_ASSERT(header.typeVersion == TD_MEDESCRIPTORTESTASSET.version);

    meSerializeResult deserializeResult = {};
    DeserializeContext deserializeCtx = {};
    deserializeCtx.mode = meSerializationMode_Binary;
    deserializeCtx.typeDesc = &TD_MEDESCRIPTORTESTASSET;
    deserializeCtx.externalDataAllocator = allocator;
    deserializeCtx.sourceData = serializeCtx.serializedData;
    deserializeCtx.outputData = meSpan(&restored, sizeof(restored));
    deserializeCtx.outResult = &deserializeResult;
    DeserializeBlocking(deserializeCtx);
    ME_ASSERT(deserializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));
    ME_ASSERT(restored.displayName.data != original.displayName.data);
    ME_ASSERT(restored.children.data != original.children.data);

    MEFREE(allocator, serializeCtx.serializedData.data);
    DestroyDescriptorTestAsset(original);
    DestroyDescriptorTestAsset(restored);
}

static void DescriptorTestDeepCopy(meAllocator* allocator)
{
    meDescriptorTestAsset original = MakeDescriptorTestAsset(allocator);
    meDescriptorTestAsset copied = {};

    DeepCopyContext ctx = {};
    ctx.srcData = &original;
    ctx.outputData = meSpan(&copied, sizeof(copied));
    ctx.allocator = allocator;
    meTypeDescriptorDeepCopy(TD_MEDESCRIPTORTESTASSET, ctx);

    ME_ASSERT(meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &copied));
    ME_ASSERT(copied.displayName.data != original.displayName.data);
    ME_ASSERT(copied.children.data != original.children.data);

    copied.displayName.data[0] = 'D';
    copied.children[0].id = 42;
    ME_ASSERT(original.displayName[0] == 'd');
    ME_ASSERT(original.children[0].id == 1);
    ME_ASSERT(!meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &copied));

    DestroyDescriptorTestAsset(original);
    DestroyDescriptorTestAsset(copied);
}

static void DescriptorTestGeneratedDeepCopyNestedOwnership(meAllocator* allocator)
{
    meDescriptorLifecycleContainer original = MakeLifecycleContainer(allocator);
    meDescriptorLifecycleContainer copied = {};

    DeepCopyContext ctx = {};
    ctx.srcData = &original;
    ctx.outputData = meSpan(&copied, sizeof(copied));
    ctx.allocator = allocator;
    meTypeDescriptorDeepCopy(TD_MEDESCRIPTORLIFECYCLECONTAINER, ctx);

    ME_ASSERT(meFieldsEqual(TD_MEDESCRIPTORLIFECYCLECONTAINER, &original, &copied));
    ME_ASSERT(copied.name.data != original.name.data);
    ME_ASSERT(copied.aliases.data != original.aliases.data);
    ME_ASSERT(DynArrayGetSize(copied.aliases) == 2);
    ME_ASSERT(copied.aliases[0].data != original.aliases[0].data);
    ME_ASSERT(copied.aliases[1].data != original.aliases[1].data);

    copied.name.data[0] = 'L';
    copied.aliases[0].data[0] = 'F';
    ME_ASSERT(original.name[0] == 'l');
    ME_ASSERT(original.aliases[0][0] == 'f');
    ME_ASSERT(!meFieldsEqual(TD_MEDESCRIPTORLIFECYCLECONTAINER, &original, &copied));

    DestroyContext destroyCtx = {};
    destroyCtx.allocator = allocator;
    destroyCtx.data = &original;
    meTypeDescriptorDestroy(TD_MEDESCRIPTORLIFECYCLECONTAINER, destroyCtx);
    destroyCtx.data = &copied;
    meTypeDescriptorDestroy(TD_MEDESCRIPTORLIFECYCLECONTAINER, destroyCtx);
}

static void DescriptorTestSerializedHeaderLifecycle(meAllocator* allocator)
{
    meSerializedHeader original = {};
    original.assetHeader = MAID(111, MAEntity);
    original.parentAsset = MAID(222, MAScene);
    original.dependencies = DynArrayCreate<MAID>(allocator);
    DynArrayPush(original.dependencies, MAID(333, MAMesh));
    DynArrayPush(original.dependencies, MAID(444, MAMaterial));

    meSerializedHeader copied = {};
    DeepCopyContext copyCtx = {};
    copyCtx.srcData = &original;
    copyCtx.outputData = meSpan(&copied, sizeof(copied));
    copyCtx.allocator = allocator;
    meTypeDescriptorDeepCopy(TD_MESERIALIZEDHEADER, copyCtx);

    ME_ASSERT(meFieldsEqual(TD_MESERIALIZEDHEADER, &original, &copied));
    ME_ASSERT(copied.dependencies.data != original.dependencies.data);
    ME_ASSERT(DynArrayGetSize(copied.dependencies) == 2);

    copied.dependencies[0] = MAID(555, MATexture);
    ME_ASSERT(original.dependencies[0] == MAID(333, MAMesh));
    ME_ASSERT(!meFieldsEqual(TD_MESERIALIZEDHEADER, &original, &copied));

    DestroyContext destroyCtx = {};
    destroyCtx.allocator = allocator;
    destroyCtx.data = &original;
    meTypeDescriptorDestroy(TD_MESERIALIZEDHEADER, destroyCtx);
    ME_ASSERT(!original.dependencies);

    destroyCtx.data = &copied;
    meTypeDescriptorDestroy(TD_MESERIALIZEDHEADER, destroyCtx);
    ME_ASSERT(!copied.dependencies);
}

static void DescriptorTestDestroy(meAllocator* allocator)
{
    meDescriptorTestAsset asset = MakeDescriptorTestAsset(allocator);
    ME_ASSERT(asset.displayName.data);
    ME_ASSERT(asset.children.data);

    DestroyContext ctx = {};
    ctx.data = &asset;
    ctx.allocator = allocator;
    meTypeDescriptorDestroy(TD_MEDESCRIPTORTESTASSET, ctx);

    ME_ASSERT(!asset.displayName.data);
    ME_ASSERT(!asset.children.data);

    meDescriptorLifecycleContainer lifecycle = MakeLifecycleContainer(allocator);
    ctx.data = &lifecycle;
    meTypeDescriptorDestroy(TD_MEDESCRIPTORLIFECYCLECONTAINER, ctx);
    ME_ASSERT(!lifecycle.name.data);
    ME_ASSERT(!lifecycle.aliases.data);
}

static void DescriptorTestGeneratedDestroyUsesMemberDestroy(meAllocator* allocator)
{
    u32 originalDestroyCalls = g_descriptorLifecycleMemberDestroyCalls;
    meDescriptorLifecycleWithDestroy data = MakeLifecycleWithDestroy(allocator);

    DestroyContext ctx = {};
    ctx.data = &data;
    ctx.allocator = allocator;
    meTypeDescriptorDestroy(TD_MEDESCRIPTORLIFECYCLEWITHDESTROY, ctx);

    ME_ASSERT(g_descriptorLifecycleMemberDestroyCalls == originalDestroyCalls + 1);
    ME_ASSERT(!data.name.data);
    ME_ASSERT(!data.destroyAliases.data);
}

static void DescriptorTestMeFsPathManualLifecycleOverrides(meAllocator* allocator)
{
    meFsPath original = {};
    meFsPath copied = {};
    ((String&)original) = String(STRING_LIT("assets/models/example.gltf"), allocator);

    DeepCopyContext copyCtx = {};
    copyCtx.srcData = &original;
    copyCtx.outputData = meSpan(&copied, sizeof(copied));
    copyCtx.allocator = allocator;
    meTypeDescriptorDeepCopy(TD_MEFSPATH, copyCtx);

    ME_ASSERT(meFieldsEqual(TD_MEFSPATH, &original, &copied));
    ME_ASSERT(copied.data != original.data);

    copied.data[0] = 'A';
    ME_ASSERT(original.data[0] == 'a');
    ME_ASSERT(!meFieldsEqual(TD_MEFSPATH, &original, &copied));

    meOwningSpan serialized = {};
    ME_ASSERT(SerializeForTest(TD_MEFSPATH, meSerializationMode_Text, &original, allocator, serialized) == meSerializeResult::SER_SUCCESS);

    meFsPath restored = {};
    ME_ASSERT(DeserializeForTest(TD_MEFSPATH, meSerializationMode_Text, serialized, &restored, allocator) == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(meFieldsEqual(TD_MEFSPATH, &original, &restored));
    ME_ASSERT(restored.data != original.data);

    MEFREE(allocator, serialized.data);

    DestroyContext destroyCtx = {};
    destroyCtx.allocator = allocator;
    destroyCtx.data = &original;
    meTypeDescriptorDestroy(TD_MEFSPATH, destroyCtx);
    ME_ASSERT(!original.data);

    destroyCtx.data = &copied;
    meTypeDescriptorDestroy(TD_MEFSPATH, destroyCtx);
    ME_ASSERT(!copied.data);

    destroyCtx.data = &restored;
    meTypeDescriptorDestroy(TD_MEFSPATH, destroyCtx);
    ME_ASSERT(!restored.data);
}

static void DescriptorTestContainerFunctions(meAllocator* allocator)
{
    meDescriptorTestAsset asset = MakeDescriptorTestAsset(allocator);
    const meTypeDescriptor& childrenField = *FindDescriptorTestField(STRING_LIT("children"));
    const meTypeDescriptor& containerType = *childrenField.thisType;

    ME_ASSERT(containerType.iterateContentFn);
    ME_ASSERT(containerType.pushElementFn);
    ME_ASSERT(containerType.removeElementFn);

    struct IterateCtx
    {
        u32 count = 0;
        s32 ids[4] = {};
    };
    IterateCtx iterateCtx = {};
    containerType.iterateContentFn(
        &asset.children,
        &childrenField,
        +[](void* elementData, const meTypeDescriptor* elementType, meContainerKey key, void* userData)
        {
            IterateCtx& ctx = *(IterateCtx*)userData;
            ME_ASSERT(elementType == &TD_MEDESCRIPTORTESTCHILD);
            ME_ASSERT(key == ctx.count);
            ctx.ids[ctx.count++] = ((meDescriptorTestChild*)elementData)->id;
        },
        &iterateCtx);
    ME_ASSERT(iterateCtx.count == 2);
    ME_ASSERT(iterateCtx.ids[0] == 1);
    ME_ASSERT(iterateCtx.ids[1] == 2);

    meDescriptorTestChild pushed = { .id = 3, .weight = 2.5f, .enabled = true };
    containerType.pushElementFn(&asset.children, &childrenField, &pushed);
    ME_ASSERT(DynArrayGetSize(asset.children) == 3);
    ME_ASSERT(asset.children[2].id == 3);

    containerType.removeElementFn(&asset.children, &childrenField, 1);
    ME_ASSERT(DynArrayGetSize(asset.children) == 2);
    ME_ASSERT(asset.children[0].id == 1);
    ME_ASSERT(asset.children[1].id == 3);

    DestroyDescriptorTestAsset(asset);
}

static void DescriptorTestDispatchOrdering(meAllocator* allocator)
{
    meDescriptorDispatchWrapper original = { { 42 } };
    meDescriptorDispatchWrapper restored = {};

    DescriptorDispatchResetStats();
    SerializeContext serializeCtx = {};
    serializeCtx.mode = meSerializationMode_Text;
    serializeCtx.typeDesc = &TD_DESCRIPTOR_DISPATCH_WRAPPER;
    serializeCtx.allocator = allocator;
    serializeCtx.sourceData = meSpan(&original, sizeof(original));
    meSerializeResult serializeResult = SerializeBlocking(serializeCtx);
    ME_ASSERT(serializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(g_descriptorDispatchStats.aliasSerializer > 0);
    ME_ASSERT(g_descriptorDispatchStats.baseSerializer == 0);

    meSerializeResult deserializeResult = {};
    DeserializeContext deserializeCtx = {};
    deserializeCtx.mode = meSerializationMode_Text;
    deserializeCtx.typeDesc = &TD_DESCRIPTOR_DISPATCH_WRAPPER;
    deserializeCtx.externalDataAllocator = allocator;
    deserializeCtx.sourceData = serializeCtx.serializedData;
    deserializeCtx.outputData = meSpan(&restored, sizeof(restored));
    deserializeCtx.outResult = &deserializeResult;
    DeserializeBlocking(deserializeCtx);
    ME_ASSERT(deserializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(restored.field.value == original.field.value);
    ME_ASSERT(g_descriptorDispatchStats.aliasDeserializer == 1);
    ME_ASSERT(g_descriptorDispatchStats.baseDeserializer == 0);
    MEFREE(allocator, serializeCtx.serializedData.data);

    DescriptorDispatchResetStats();
    meDescriptorDispatchWrapper semanticallyEqual = { { 52 } };
    ME_ASSERT(meFieldsEqual(TD_DESCRIPTOR_DISPATCH_WRAPPER, &original, &semanticallyEqual));
    ME_ASSERT(g_descriptorDispatchStats.aliasEquals == 1);
    ME_ASSERT(g_descriptorDispatchStats.baseEquals == 0);

    DescriptorDispatchResetStats();
    meDescriptorDispatchWrapper copied = {};
    DeepCopyContext copyCtx = {};
    copyCtx.srcData = &original;
    copyCtx.outputData = meSpan(&copied, sizeof(copied));
    copyCtx.allocator = allocator;
    meTypeDescriptorDeepCopy(TD_DESCRIPTOR_DISPATCH_WRAPPER, copyCtx);
    ME_ASSERT(copied.field.value == original.field.value + 1000);
    ME_ASSERT(g_descriptorDispatchStats.aliasDeepCopy == 1);
    ME_ASSERT(g_descriptorDispatchStats.baseDeepCopy == 0);

    DescriptorDispatchResetStats();
    meDescriptorDispatchWrapper destroyed = { { 42 } };
    DestroyContext destroyCtx = {};
    destroyCtx.data = &destroyed;
    destroyCtx.allocator = allocator;
    meTypeDescriptorDestroy(TD_DESCRIPTOR_DISPATCH_WRAPPER, destroyCtx);
    ME_ASSERT(destroyed.field.value == -1000);
    ME_ASSERT(g_descriptorDispatchStats.aliasDestroy == 1);
    ME_ASSERT(g_descriptorDispatchStats.baseDestroy == 0);
}

void meDescriptorTests()
{
    LOG_INFO("Testing meTypeDescriptor serialization and operations...");

    meAllocator* allocator = GetDefaultAllocator();
    LOG_INFO("Testing generated descriptor lifecycle hooks...");
    DescriptorTestGeneratedLifecycleHookPresence();
    LOG_INFO("Testing type descriptor registry...");
    DescriptorTestTypeDescriptorRegistry();
    LOG_INFO("Testing setToDefaultsFn...");
    DescriptorTestSetToDefaults();
    LOG_INFO("Testing serialization, deserialization, and equals...");
    DescriptorTestSerializeDeserializeEquals(allocator);
    LOG_INFO("Testing binary serialization, deserialization, and equals...");
    DescriptorTestBinarySerializeDeserializeEquals(allocator);
    LOG_INFO("Testing text serialization fixture edge cases...");
    DescriptorTestSerializationFixtureRoundTrip(allocator, meSerializationMode_Text);
    LOG_INFO("Testing text serialization rejects invalid UTF-8 bytes...");
    DescriptorTestTextInvalidUtf8BytesFail(allocator);
    LOG_INFO("Testing binary serialization fixture edge cases...");
    DescriptorTestSerializationFixtureRoundTrip(allocator, meSerializationMode_Binary);
    LOG_INFO("Testing empty strings and containers in text serialization...");
    DescriptorTestEmptyValuesRoundTrip(allocator, meSerializationMode_Text);
    LOG_INFO("Testing empty strings and containers in binary serialization...");
    DescriptorTestEmptyValuesRoundTrip(allocator, meSerializationMode_Binary);
    LOG_INFO("Testing wrong serialization mode failures...");
    DescriptorTestWrongModeFails(allocator);
    LOG_INFO("Testing truncated text and binary data failures...");
    DescriptorTestTruncatedDataFails(allocator, meSerializationMode_Text);
    DescriptorTestTruncatedDataFails(allocator, meSerializationMode_Binary);
    LOG_INFO("Testing version mismatch failures...");
    DescriptorTestVersionMismatchFails(allocator, meSerializationMode_Text);
    DescriptorTestVersionMismatchFails(allocator, meSerializationMode_Binary);
    LOG_INFO("Testing type mismatch failures...");
    DescriptorTestTypeMismatchFails(allocator, meSerializationMode_Text);
    DescriptorTestTypeMismatchFails(allocator, meSerializationMode_Binary);
    LOG_INFO("Testing deepCopyFn...");
    DescriptorTestDeepCopy(allocator);
    LOG_INFO("Testing generated deepCopyFn with nested owned fields...");
    DescriptorTestGeneratedDeepCopyNestedOwnership(allocator);
    LOG_INFO("Testing serialized header lifecycle hooks...");
    DescriptorTestSerializedHeaderLifecycle(allocator);
    LOG_INFO("Testing destroyFn...");
    DescriptorTestDestroy(allocator);
    LOG_INFO("Testing generated destroyFn member dispatch...");
    DescriptorTestGeneratedDestroyUsesMemberDestroy(allocator);
    LOG_INFO("Testing meFsPath manual lifecycle overrides...");
    DescriptorTestMeFsPathManualLifecycleOverrides(allocator);
    LOG_INFO("Testing iterateContentFn, pushElementFn, and removeElementFn...");
    DescriptorTestContainerFunctions(allocator);
    LOG_INFO("Testing descriptor dispatch ordering...");
    DescriptorTestDispatchOrdering(allocator);

    LOG_INFO("meTypeDescriptor tests complete");
}
