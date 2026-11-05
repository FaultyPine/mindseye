#pragma once

#include "core/me_string.h"

struct meMesh;
struct meMaterial;
struct meTexture;
struct cgltf_mesh;
struct cgltf_material;
struct cgltf_image;

bool meAssetImportGltfMesh(meMesh& outMesh, StringView sourceDirectory, const cgltf_mesh& mesh);
bool meAssetImportGltfMaterial(meMaterial& material, StringView sourceDirectory, const cgltf_material& source);
bool meAssetImportGltfTexture(meTexture& texture, StringView sourceDirectory, const cgltf_image& image);
