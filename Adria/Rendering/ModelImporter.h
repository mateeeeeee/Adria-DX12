#pragma once
#include <DirectXMath.h>
#include <optional>
#include <array>
#include <vector>
#include <string>
#include "Components.h"
#include "../Core/Definitions.h"
#include "entt/entity/registry.hpp"
#include "../Math/ComputeNormals.h"
#include "../Utilities/Heightmap.h"
#include "../Events/Delegate.h"

struct ID3D12Device;

namespace adria
{

	enum class ELightMesh
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
		bool used_in_rt = true;
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
		ENormalCalculation normal_type = ENormalCalculation::None;
		std::unique_ptr<Heightmap> heightmap = nullptr;
	};
	struct OceanParameters
	{
		GridParameters ocean_grid;
	};

    struct LightParameters
    {
        Light light_data;
        ELightMesh mesh_type = ELightMesh::NoMesh;
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

    class TextureManager;
    class GraphicsDevice;
 
	class ModelImporter
	{
		[[nodiscard]] std::vector<entt::entity> LoadGrid(GridParameters const&);
		[[nodiscard]] std::vector<entt::entity> LoadObjMesh(std::string const&);
	public:
        
        ModelImporter(entt::registry& reg, GraphicsDevice* device, TextureManager& texture_manager);

		[[maybe_unused]] entt::entity LoadSkybox(SkyboxParameters const&);
        [[maybe_unused]] entt::entity LoadLight(LightParameters const&);
		[[maybe_unused]] std::vector<entt::entity> LoadOcean(OceanParameters const&);
		[[maybe_unused]] entt::entity LoadDecal(DecalParameters const&);

		[[maybe_unused]] std::vector<entt::entity> ImportModel_GLTF(ModelParameters const&);
	private:
        entt::registry& reg;
        GraphicsDevice* gfx;
        TextureManager& texture_manager;
	};
}

