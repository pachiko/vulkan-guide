#pragma once
#include <vk_types.h>
#include <unordered_map>
#include <filesystem>

// Material has pipeline and DSet
struct GLTFMaterial {
	MaterialInstance data;
};

// A geometry surface / primitive
struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
	std::shared_ptr<GLTFMaterial> material;
};

// GLTF Mesh
struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

//forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);