#pragma once

#include "core/me_string.h"

struct OSFileReference;
struct meMesh;
struct meMaterial;
struct meTexture;
struct meScene;
struct meAsset;
struct cgltf_mesh;
struct cgltf_material;
struct cgltf_image;
struct cgltf_scene;

bool meAssetImportGltfScene(
    meAsset& outAsset, 
    OSFileReference& importThis);

bool meAssetImportGltfMesh(
    meAsset& outAsset, 
    OSFileReference& importThis);

bool meAssetImportGltfMaterial(
    meMaterial& material, 
    StringView sourceDirectory, 
    const cgltf_material& source);

bool meAssetImportGltfTexture(
    meTexture& texture, 
    StringView sourceDirectory,
    const cgltf_image& image);
