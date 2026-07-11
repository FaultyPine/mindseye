#include "tests/me_descriptor_tests.h"

#include "core/me_log.h"
#include "core/me_serialize.h"
#include "external/json.hpp"
#include "generatedtypes/me_descriptor_tests.generated.h"

using json = nlohmann::json;

struct meDescriptorDispatchValue
{
    s32 value = 0;
};

struct meDescriptorDispatchWrapper
{
    meDescriptorDispatchValue field = {};
};

static void DescriptorDispatchBaseSerializer(const meTypeDescriptor&, SerializeContext& ctx);
static void DescriptorDispatchAliasSerializer(const meTypeDescriptor&, SerializeContext& ctx);
static bool DescriptorDispatchBaseDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
static bool DescriptorDispatchAliasDeserializer(const meTypeDescriptor&, DeserializeContext& ctx);
static bool DescriptorDispatchBaseEquals(const meTypeDescriptor&, const void* a, const void* b);
static bool DescriptorDispatchAliasEquals(const meTypeDescriptor&, const void* a, const void* b);
static void DescriptorDispatchBaseDeepCopy(const meTypeDescriptor&, DeepCopyContext& ctx);
static void DescriptorDispatchAliasDeepCopy(const meTypeDescriptor&, DeepCopyContext& ctx);

static meTypeDescriptor TD_DESCRIPTOR_DISPATCH_BASE = {
    .name = STRING_LIT("meDescriptorDispatchValue"),
    .size = sizeof(meDescriptorDispatchValue),
    .align = alignof(meDescriptorDispatchValue),
    .serializerFn = DescriptorDispatchBaseSerializer,
    .deserializerFn = DescriptorDispatchBaseDeserializer,
    .equalsFn = DescriptorDispatchBaseEquals,
    .deepCopyFn = DescriptorDispatchBaseDeepCopy,
};

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
        .deepCopyFn = DescriptorDispatchAliasDeepCopy,
    },
};

static meTypeDescriptor TD_DESCRIPTOR_DISPATCH_WRAPPER = {
    .name = STRING_LIT("meDescriptorDispatchWrapper"),
    .fields = { g_descriptorDispatchWrapperFields },
    .size = sizeof(meDescriptorDispatchWrapper),
    .align = alignof(meDescriptorDispatchWrapper),
};

struct DescriptorDispatchStats
{
    u32 baseSerializer = 0;
    u32 aliasSerializer = 0;
    u32 baseDeserializer = 0;
    u32 aliasDeserializer = 0;
    u32 baseEquals = 0;
    u32 aliasEquals = 0;
    u32 baseDeepCopy = 0;
    u32 aliasDeepCopy = 0;
};

static DescriptorDispatchStats g_descriptorDispatchStats = {};

static void DescriptorDispatchResetStats()
{
    g_descriptorDispatchStats = {};
}

static void DescriptorDispatchBaseSerializer(const meTypeDescriptor&, SerializeContext& ctx)
{
    g_descriptorDispatchStats.baseSerializer++;
    const meDescriptorDispatchValue& value = *(const meDescriptorDispatchValue*)ctx.data.data;
    *(json*)ctx.outputData.data = value.value + 2000;
}

static void DescriptorDispatchAliasSerializer(const meTypeDescriptor&, SerializeContext& ctx)
{
    g_descriptorDispatchStats.aliasSerializer++;
    const meDescriptorDispatchValue& value = *(const meDescriptorDispatchValue*)ctx.data.data;
    *(json*)ctx.outputData.data = value.value + 1000;
}

static bool DescriptorDispatchBaseDeserializer(const meTypeDescriptor&, DeserializeContext& ctx)
{
    g_descriptorDispatchStats.baseDeserializer++;
    StringView input = StringView::FromSpan(ctx.inputData);
    ((meDescriptorDispatchValue*)ctx.outputData.data)->value = StringParseInt32(input) - 2000;
    return true;
}

static bool DescriptorDispatchAliasDeserializer(const meTypeDescriptor&, DeserializeContext& ctx)
{
    g_descriptorDispatchStats.aliasDeserializer++;
    StringView input = StringView::FromSpan(ctx.inputData);
    ((meDescriptorDispatchValue*)ctx.outputData.data)->value = StringParseInt32(input) - 1000;
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
    asset.header = MAID(12345, MAEntity);
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

static void DestroyDescriptorTestAsset(meDescriptorTestAsset& asset)
{
    DynArrayDestroy(asset.children);
}

static const meTypeDescriptor* FindDescriptorTestField(StringView name)
{
    const meTypeDescriptor* result = nullptr;
    for (u32 i = 0; i < TD_MEDESCRIPTORTESTASSET.fields.size; i++)
    {
        meTypeDescriptor& member = TD_MEDESCRIPTORTESTASSET.fields[i];
        if (StringCompare(member.name, name))
        {
            result = &member;
            break;
        }
    }
    ME_ASSERT(result);
    return result;
}

static void DescriptorTestSetToDefaults()
{
    alignas(meDescriptorTestAsset) u8 backing[sizeof(meDescriptorTestAsset)];
    ME_MEMSET(backing, 0xCD, sizeof(backing));

    TD_MEDESCRIPTORTESTASSET.setToDefaultsFn(backing);
    meDescriptorTestAsset& asset = *(meDescriptorTestAsset*)backing;

    ME_ASSERT(!asset.header);
    ME_ASSERT(!asset.displayName);
    ME_ASSERT(asset.health == 0);
    ME_ASSERT(asset.speed == 0.0f);
    ME_ASSERT(asset.samples[0] == 0);
    ME_ASSERT(asset.samples[1] == 0);
    ME_ASSERT(asset.samples[2] == 0);
    ME_ASSERT(!asset.children);

    asset.~meDescriptorTestAsset();
}

static void DescriptorTestSerializeDeserializeEquals(meAllocator* allocator)
{
    meDescriptorTestAsset original = MakeDescriptorTestAsset(allocator);
    meDescriptorTestAsset restored = {};

    StringView serialized = {};
    meSerializeResult serializeResult = SerializeToTextBlocking(
        TD_MEDESCRIPTORTESTASSET,
        &original,
        allocator,
        serialized);
    ME_ASSERT(serializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(serialized);

    meSerializeResult deserializeResult = {};
    DeserializeFromTextBlocking(
        TD_MEDESCRIPTORTESTASSET,
        allocator,
        serialized,
        meSpan(&restored, sizeof(restored)),
        deserializeResult);
    ME_ASSERT(deserializeResult == meSerializeResult::SER_SUCCESS);

    ME_ASSERT(meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));
    ME_ASSERT(!TD_MEDESCRIPTORTESTASSET.equalsFn);

    meSpan headerSpan = meSerializeTryGetAssetHeader(
        TD_MEDESCRIPTORTESTASSET,
        meSpan(&restored, sizeof(restored)));
    ME_ASSERT(headerSpan.data);
    ME_ASSERT(*(MAID*)headerSpan.data == original.header);

    restored.samples[1]++;
    ME_ASSERT(!meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));
    restored.samples[1] = original.samples[1];

    restored.children[1].weight += 1.0f;
    ME_ASSERT(!meFieldsEqual(TD_MEDESCRIPTORTESTASSET, &original, &restored));

    MEFREE(allocator, serialized.data);
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
    StringView serialized = {};

    DescriptorDispatchResetStats();
    meSerializeResult serializeResult = SerializeToTextBlocking(
        TD_DESCRIPTOR_DISPATCH_WRAPPER,
        &original,
        allocator,
        serialized);
    ME_ASSERT(serializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(g_descriptorDispatchStats.aliasSerializer == 1);
    ME_ASSERT(g_descriptorDispatchStats.baseSerializer == 0);

    meSerializeResult deserializeResult = {};
    DeserializeFromTextBlocking(
        TD_DESCRIPTOR_DISPATCH_WRAPPER,
        allocator,
        serialized,
        meSpan(&restored, sizeof(restored)),
        deserializeResult);
    ME_ASSERT(deserializeResult == meSerializeResult::SER_SUCCESS);
    ME_ASSERT(restored.field.value == original.field.value);
    ME_ASSERT(g_descriptorDispatchStats.aliasDeserializer == 1);
    ME_ASSERT(g_descriptorDispatchStats.baseDeserializer == 0);
    MEFREE(allocator, serialized.data);

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
}

void meDescriptorTests()
{
    LOG_INFO("Testing meTypeDescriptor serialization and operations...");

    meAllocator* allocator = GetDefaultAllocator();
    LOG_INFO("Testing setToDefaultsFn...");
    DescriptorTestSetToDefaults();
    LOG_INFO("Testing serialization, deserialization, and equals...");
    DescriptorTestSerializeDeserializeEquals(allocator);
    LOG_INFO("Testing deepCopyFn...");
    DescriptorTestDeepCopy(allocator);
    LOG_INFO("Testing iterateContentFn, pushElementFn, and removeElementFn...");
    DescriptorTestContainerFunctions(allocator);
    LOG_INFO("Testing descriptor dispatch ordering...");
    DescriptorTestDispatchOrdering(allocator);

    LOG_INFO("meTypeDescriptor tests complete");
}
