#pragma once
#include <DirectXMath.h>
#include <optional>
#include <array>
#include <vector>
#include <string>
#include "Components.h"
#include "entt/entity/registry.hpp"
#include "Core/CoreTypes.h"
#include "Utilities/Heightmap.h"
#include "Math/ComputeNormals.h"

namespace adria
{
	enum class LightMesh
	{
		NoMesh,
		Quad,
		Sphere
	};

    struct ModelParameters
    {
        std::string model_path = "";
        std::string textures_path = "";
		DirectX::XMMATRIX model_matrix;
		bool used_in_raytracing = true;
		bool merge_meshes = true;
    };
    struct SkyboxParameters
    {
        std::optional<std::wstring> cubemap;
        std::array<std::wstring, 6> cubemap_textures;
    };
	struct GridParameters
	{
		uint64 tile_count_x;
		uint64 tile_count_z;
		float tile_size_x;
		float tile_size_z;
		float texture_scale_x;
		float texture_scale_z;
		uint64 chunk_count_x;
		uint64 chunk_count_z;
		bool split_to_chunks = false;
		NormalCalculation normal_type = NormalCalculation::None;
		std::unique_ptr<Heightmap> heightmap;
	};
	struct OceanParameters
	{
		GridParameters ocean_grid;
	};

    struct LightParameters
    {
        Light light_data;
        LightMesh mesh_type = LightMesh::NoMesh;
        uint32 mesh_size = 0u;
        std::optional<std::wstring> light_texture = std::nullopt;
    };
	struct DecalParameters
	{
		std::string name = "Decal";
		std::string albedo_texture_path = "Resources/Textures/Decals/Decal_00_Albedo.tga";
		std::string normal_texture_path = "Resources/Textures/Decals/Decal_00_Normal.png";
		float rotation = 0.0f;
		float size = 50.0f;
		bool modify_gbuffer_normals = false;
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT4 normal;
	};

    class GfxDevice;
 
	class EntityLoader
	{
		[[nodiscard]] std::vector<entt::entity> LoadGrid(GridParameters const&);
		[[nodiscard]] std::vector<entt::entity> LoadObjMesh(std::string const&);
	public:
        
        EntityLoader(entt::registry& reg, GfxDevice* device);
		~EntityLoader();

		[[maybe_unused]] entt::entity LoadSkybox(SkyboxParameters const&);
        [[maybe_unused]] entt::entity LoadLight(LightParameters const&);
		[[maybe_unused]] std::vector<entt::entity> LoadOcean(OceanParameters const&);
		[[maybe_unused]] entt::entity LoadDecal(DecalParameters const&);

		[[maybe_unused]] entt::entity ImportModel_GLTF(ModelParameters const&);
	private:
        entt::registry& reg;
        GfxDevice* gfx;
	};
}

