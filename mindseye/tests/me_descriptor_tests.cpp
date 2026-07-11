#include "tests/me_descriptor_tests.h"

#include "core/me_log.h"
#include "core/me_serialize.h"
#include "generatedtypes/me_descriptor_tests.generated.h"

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
    meTypeDescriptorWalkMembers(TD_MEDESCRIPTORTESTASSET, nullptr,
        [&](const meTypeDescriptorMember& member)
        {
            if (StringCompare(member.field.name, name))
            {
                result = &member.field;
                return false;
            }
            return true;
        },
        false);
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
    ME_ASSERT(TD_MEDESCRIPTORTESTASSET.equalsFn(TD_MEDESCRIPTORTESTASSET, &original, &restored));

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

    LOG_INFO("meTypeDescriptor tests complete");
}
