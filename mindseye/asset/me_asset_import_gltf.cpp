#include "asset/me_asset_import_gltf.h"
#include "core/me_app.h"
#include "core/me_profile.h"
#include "external/stb/stb_image.h"
#include "external/cgltf.h"
#include "render/me_material.h"
#include "render/me_mesh.h"
#include "render/me_texture.h"
#include "scene/me_scene.h"
#include "core/me_scope_exit.h"

ME_DEFINE_LOG_CATEGORY(AssetImportGltf);

static cgltf_material GenerateDummyGLTFMaterial()
{
    cgltf_material mat;
    ME_MEMCLEAR(&mat, sizeof(mat));
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wwritable-strings"
    mat.name = "NoMaterial";
    #pragma clang diagnostic pop
    return mat;
}

String AssetNameFromImportFilename(StringView path)
{
    StringView filename = meFsGetFileFromFullPath(path);
    StringView fileNoExt = meFsRemoveExt(filename);
    return fileNoExt;
}

static bool ImportDestinationAvailable(StringView name, meAssetType type)
{
    String path = meAssetGetAbsPathForResource(meAssetEnsurePathHasGoodExtension(name, type));
    OSFileReference file;
    file.InitWithoutOpening(path);
    if (meOSFileExists(file))
    {
        LOG_ERROR(AssetImportGltf, "Cannot import asset. destination already exists: {}", LogSV(path));
        return false;
    }
    return true;
}

bool meAssetImportGltfTexture(
    meTexture& texture, 
    StringView gltfResPath, 
    const cgltf_image& image)
{
    ME_PROFILE_FUNCTION();
    // not supporting embedded texture data rn
    if (!image.uri || image.buffer_view || FindInString(StringFromCString(image.uri), STRING_LIT("data:")) == 0)
    {
        return false;
    }
    StringView directory = gltfResPath;
    StringView path = StringFormatTmp("%.*s/%s", STRING_VAARGS(directory), image.uri);
    OSFileReference file;
    if (!meOSOpenFile(file, path, OSFileFlags_OnlyIfExists | OSFileFlags_ReadOnly | OSFileFlags_ScopedFile))
    {
        return false;
    }
    ScopedAllocation bytes(meOSGetFileSize(file), GetDefaultAllocator());
    if (!meOSReadFileContents(file, bytes, bytes.size))
    {
        return false;
    }
    s32 width = 0, height = 0, channels = 0;
    u8* pixels = stbi_load_from_memory((u8*)bytes.data, bytes.size, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
    {
        LOG_ERROR(AssetImportGltf, "Failed to decode image {}: {}", LogSV(path), stbi_failure_reason());
        return false;
    }
    texture.width = width;
    texture.height = height;
    texture.buffer.cpuData = MEALLOC(meTextureGetPool().GetPayloadAllocator(), (u64)width * height * 4);
    ME_MEMCPY(texture.buffer.cpuData.data, pixels, texture.buffer.cpuData.size);
    stbi_image_free(pixels);
    return true;
}

bool meAssetImportGltfMaterial(
    meMaterial& material, 
    StringView gltfResPath, 
    const cgltf_material& gltfMaterial)
{
    ME_PROFILE_FUNCTION();
    StringCopy(StringView(material.name, meMaterial::MEMATERIAL_MAX_NAME_LEN), StringFromCString(gltfMaterial.name));
    material.textureHandles[Diffuse] = meTextureGetPool().plainWhite1x1;
    // TODO: the other meMaterialTextureTypes...
    if (gltfMaterial.has_pbr_metallic_roughness)
    {
        meTexturePool& textures = meTextureGetPool();
        meTextureID textureHandle = textures.Load({.resourceType = meResourceType_InstanceAsset});
        meTexture& texture = textures.Get(textureHandle);
        const cgltf_texture* image = gltfMaterial.pbr_metallic_roughness.base_color_texture.texture;
        bool success = true;
        if (image && image->image)
        {
            success = meAssetImportGltfTexture(texture, gltfResPath, *image->image);
            StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), StringFromCString(image->name));
        }
        else
        {
            const float* rgba = gltfMaterial.pbr_metallic_roughness.base_color_factor;
            u32* pixel = MEALLOC(textures.GetPayloadAllocator(), sizeof(u32));
            *pixel = PackFloatsToU32(rgba[0], rgba[1], rgba[2], rgba[3]);
            texture.buffer.cpuData = meSpan(pixel, sizeof(u32));
            texture.width = texture.height = 1;
            StringCopy(StringView(texture.name, meTexture::METEXTURE_MAX_NAME_LEN), STRING_LIT("Static Color Texture"));
        }
        if (success)
        {
            meAsset textureAsset(textureHandle, MATexture);
            textureAsset.runtimeContent = textures.GetShared(textureHandle);
            success = meAssetSystemGet().assetLoaders[MATexture]->meAssetPrepareCompiledData(textureAsset);
        }
        if (success)
        {
            material.textureHandles[Diffuse] = meTypedAsset<MATexture>(meAsset(textureHandle, MATexture));
        }
        else
        {
            textures.Destroy(textureHandle);
        }
    }
    return true;
}

bool meAssetImportGltfMesh(
    meMesh& outMesh, 
    StringView gltfResPath, 
    const cgltf_mesh& inMesh)
{
    ME_PROFILE_FUNCTION();
    meMaterialPool& materialPool = meMaterialGetPool();
    meMeshPool& meshPool = meMeshPoolGet();
    meAllocator* meshPayloadAllocator = meshPool.GetPayloadAllocator();

    outMesh.name = String(StringFromCString(inMesh.name), meshPayloadAllocator);
    BoundingBox& meshBounds = outMesh.meshBounds;
    for (u64 meshPrimIdx = 0; meshPrimIdx < inMesh.primitives_count; meshPrimIdx++)
    {
        const cgltf_primitive& prim = inMesh.primitives[meshPrimIdx];
        meMaterialID materialHandle = materialPool.Load({.resourceType = meResourceType_InstanceAsset});
        meAsset materialAsset(materialHandle, MAMaterial);
        materialAsset.runtimeContent = materialPool.GetShared(materialHandle);
        cgltf_material fallback = GenerateDummyGLTFMaterial();
        if (!meAssetImportGltfMaterial(materialPool.Get(materialHandle), gltfResPath, prim.material ? *prim.material : fallback))
        {
            materialPool.Destroy(materialHandle);
            return false;
        }
        outMesh.material = meTypedAsset<MAMaterial>(materialAsset);
        // attribs like position, texcoords, normals
        for (u64 attributeIdx = 0; attributeIdx < prim.attributes_count; attributeIdx++)
        {
            const cgltf_attribute& attrib = prim.attributes[attributeIdx];
            StringView attribName = StringView(attrib.name, CStringLength(attrib.name));
            if (attrib.type == cgltf_attribute_type_position)
            {
                const cgltf_accessor* accessor = attrib.data;
                u64 stride = sizeof(f32) * 3;
                u64 dataSize = stride * accessor->count;
                Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
                Allocation bumper = allocation;
                for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
                {
                    cgltf_accessor_read_float(accessor, posIdx, bumper, 3);
                    meshBounds.min = glm::min(meshBounds.min, glm::make_vec3((float*)bumper.data));
                    meshBounds.max = glm::max(meshBounds.max, glm::make_vec3((float*)bumper.data));
                    bumper = bumper.Subspan(stride);
                }
                outMesh.vertBuffer.cpuData = allocation;
            }
            else if (attrib.type == cgltf_attribute_type_normal)
            {
                const cgltf_accessor* accessor = attrib.data;
                u64 stride = sizeof(f32) * 3;
                u64 dataSize = stride * accessor->count;
                Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
                Allocation bumper = allocation;
                for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
                {
                    cgltf_accessor_read_float(accessor, posIdx, bumper, 3);
                    bumper = bumper.Subspan(stride);
                }
                outMesh.normBuffer.cpuData = allocation;
            }
            else if (attrib.type == cgltf_attribute_type_tangent)
            {
            }
            else if (attrib.type == cgltf_attribute_type_texcoord)
            {
                const cgltf_accessor* accessor = attrib.data;
                u64 stride = sizeof(f32) * 2;
                u64 dataSize = stride * accessor->count;
                Allocation allocation = MEALLOC(meshPayloadAllocator, dataSize);
                Allocation bumper = allocation;
                for (u64 posIdx = 0; posIdx < accessor->count; posIdx++)
                {
                    cgltf_accessor_read_float(accessor, posIdx, bumper, 2);
                    bumper = bumper.Subspan(stride);
                }
                outMesh.texcoordBuffer.cpuData = allocation;
            }
        }

        // indices
        if (prim.indices)
        {
            const cgltf_accessor* accessor = prim.indices;
            u64 stride = accessor->stride;
            u64 indicesMemSize = accessor->count * stride;
            Allocation indicesMemory = MEALLOC(meshPayloadAllocator, indicesMemSize);
            u8* indicesBumper = (u8*)indicesMemory.data;
            meMeshVertexLayoutType indexLayoutType = meMeshVertexLayoutType_Index16;
            switch (accessor->component_type)
            {
            case cgltf_component_type_r_16u:
                indexLayoutType = meMeshVertexLayoutType_Index16;
                break;
            case cgltf_component_type_r_32u:
                indexLayoutType = meMeshVertexLayoutType_Index32;
                break;
            default:
                ME_ASSERT(false && "Unsupported index buffer stride");
                break;
            }
            for (u64 idx = 0; idx < accessor->count; idx++)
            {
                u32 readIdx = (u32)cgltf_accessor_read_index(accessor, idx);
                switch (accessor->component_type)
                {
                case cgltf_component_type_r_8u:
                    *(u8*)indicesBumper = (u8)readIdx;
                    break;
                case cgltf_component_type_r_16u:
                    *(u16*)indicesBumper = (u16)readIdx;
                    break;
                case cgltf_component_type_r_32u:
                    *(u32*)indicesBumper = (u32)readIdx;
                    break;
                default:
                    // Handle error
                    break;
                }
                indicesBumper += stride;
            }
            outMesh.idxBuffer.cpuData = indicesMemory;
            outMesh.indexLayout = indexLayoutType;
        }
    }
    return true;
}


bool meAssetImportGltfMesh(
    meAsset& outAsset, 
    OSFileReference& importThis)
{
    ME_ASSERT(!importThis.HasOpenFile());
    meMeshPool& pool = meMeshPoolGet();

    String newAssetName = AssetNameFromImportFilename(importThis.GetPath());
    if (!ImportDestinationAvailable(newAssetName, MAMesh))
    {
        return false;
    }
    outAsset = meAssetCreateNewAsset(meAssetType::MAMesh, meResourceType::meResourceType_TemplateAsset, newAssetName);
    meMesh& outMesh = pool.Get(outAsset);
    
    String resourcePathAbs = importThis.GetPath();
    meMemoryMappedFile mapping;
    if (!meOSMapFile(mapping, resourcePathAbs, (OSFileFlags_OnlyIfExists | OSFileFlags_ScopedFile | OSFileFlags_ReadOnly)))
    {
        return false;
    }
	meSpan gltfBuffer(mapping.ptr, mapping.size);

    cgltf_options options = {};
    meSharedPtr<cgltf_data> gltfData;

    cgltf_result parseResult = {};
    {
        ME_PROFILE_SCOPE("cgltf_parse mesh");
        cgltf_data* ptr = nullptr;
        parseResult = cgltf_parse(&options, gltfBuffer.data, gltfBuffer.size, &ptr);
        gltfData = meSharedPtr<cgltf_data>(ptr, cgltf_free);
    }
    if (parseResult == cgltf_result_success)
    {
        ME_PROFILE_SCOPE("cgltf_load_buffers mesh");
        parseResult = cgltf_load_buffers(&options, gltfData.get(), resourcePathAbs.cstr());
        if (parseResult != cgltf_result_success)
        {
            LOG_WARN(AssetImportGltf, "Failed to load gltf buffers from {}", LogSV(resourcePathAbs));
            return false;
        }
    }
    else
    {
        LOG_WARN(AssetImportGltf, "Failed to parse gltf from {}", LogSV(resourcePathAbs));
        return false;
    }

    if (!gltfData->meshes_count)
    {
        return false;
    }
    if (gltfData->meshes_count > 1)
    {
        LOG_WARN(AssetImportGltf, "Loading a gltf mesh which contains multiple meshes will ignore all but the first mesh");
    }
    StringView gltfResPath = msFsGetDirFromPath(resourcePathAbs);
    return meAssetImportGltfMesh(outMesh, gltfResPath, gltfData->meshes[0]);
}

bool meAssetImportGltfScene(
    meScene& outScene, 
    StringView resourcePathAbs, 
    const cgltf_scene& scene)
{
    ME_PROFILE_FUNCTION();
	DynArray<meTypedAsset<MAEntity>>& entities = outScene.entities;
	StringView gltfResPath = msFsGetDirFromPath(resourcePathAbs);
	meMeshPool& meshPool = meMeshPoolGet();
	for (u64 nodeIdx = 0; nodeIdx < scene.nodes_count; nodeIdx++)
	{
		const cgltf_node& node = *scene.nodes[nodeIdx];
		float nodeMatrix[16];
		cgltf_node_transform_local(&node, nodeMatrix);
		meTransform nodeTf = meTransform(glm::make_mat4(nodeMatrix));
		meTypedAsset<MAEntity> entityAsset = meEntityCreateBlankInstance(StringFromCString(node.name));
		meEntity& entity = meEntityGet(entityAsset);
		entity.transform = nodeTf;
		if (node.mesh)
		{
			const cgltf_mesh& gltfmesh = *node.mesh;
			meMeshID meshHandle = meshPool.Load({.resourceType = meResourceType_InstanceAsset});
            meAsset meshAsset(meshHandle, MAMesh);
            meshAsset.runtimeContent = meshPool.GetShared(meshHandle);
            if (!meAssetImportGltfMesh(meshPool.Get(meshHandle), gltfResPath, gltfmesh))
            {
                meshPool.Destroy(meshHandle);
                return false;
            }
			meMesh& mesh = meshPool.Get(meshHandle);
            // BOOKMARK: asset import
            // TODO: right now, we have this asset without an asset id, it's just a runtime concept
            // in the future, we won't be "loading from gltf". We'll "import" gltf into mindseye assets, and load those, so this concept of a meAsset with no MAID will go away
            entity.mesh = meshAsset;
			entity.authoritativeBoundsOS = mesh.meshBounds; // may change due to anims. Default initialized to mesh bounds
		}
		DynArrayPush(entities, entityAsset);
	}
    return true;
}


bool meAssetImportGltfScene(
    meAsset& outAsset, 
    OSFileReference& importThis)
{
    ME_ASSERT(!importThis.HasOpenFile());
    meScenePool& scenePool = meScenePoolGet();

    String newAssetName = AssetNameFromImportFilename(importThis.GetPath());
    if (!ImportDestinationAvailable(newAssetName, MAScene))
    {
        return false;
    }
    outAsset = meAssetCreateNewAsset(meAssetType::MAScene, meResourceType::meResourceType_TemplateAsset, newAssetName);
    meScene& outScene = scenePool.Get(outAsset);

    String resourcePathAbs = importThis.GetPath();
	meMemoryMappedFile mapping;
    if (!meOSMapFile(mapping, resourcePathAbs, (OSFileFlags_OnlyIfExists | OSFileFlags_ScopedFile | OSFileFlags_ReadOnly)))
    {
        LOG_ERROR(AssetImportGltf, "Failed to open the file for importing " STRING_FMT, STRING_VAARGS(resourcePathAbs));
        return false;
    }
	meSpan gltfBuffer(mapping.ptr, mapping.size);

    cgltf_options options = {};
    meSharedPtr<cgltf_data> gltfData{};

	// parses the gltf json metadata
    cgltf_result parseResult = {};
	{
		ME_PROFILE_SCOPE("cgltf_parse scene");
        cgltf_data* ptr = nullptr;
		parseResult = cgltf_parse(&options, gltfBuffer.data, gltfBuffer.size, &ptr);
        gltfData = meSharedPtr<cgltf_data>(ptr, cgltf_free);
	}
    if (parseResult == cgltf_result_success)
    {
		// loads the external buffers (actual geo, textures, etc)
		{
			ME_PROFILE_SCOPE("cgltf_load_buffers scene");
			parseResult = cgltf_load_buffers(&options, gltfData.get(), resourcePathAbs.cstr());
		}
		if (parseResult != cgltf_result_success)
		{
			LOG_WARN(AssetImportGltf, "Failed to load gltf buffers from {}", LogSV(resourcePathAbs));
            return false;
		}
    }
	else
	{
		LOG_WARN(AssetImportGltf, "Failed to parse gltf from {}", LogSV(resourcePathAbs));
        return false;
	}
	if (!gltfData->scene)
    {
        return false;
    }
    return meAssetImportGltfScene(outScene, resourcePathAbs, *gltfData->scene);
}