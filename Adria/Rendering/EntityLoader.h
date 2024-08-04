#pragma once
#include <optional>
#include <array>
#include <vector>
#include <string>
#include "Components.h"
#include "Math/NormalsUtil.h"
#include "Utilities/Heightmap.h"
#include "entt/entity/registry.hpp"

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
		Matrix model_matrix;
		bool triangle_ccw = true;
		bool force_mask_alpha_usage = false;
    };
    struct SkyboxParameters
    {
        std::optional<std::string> cubemap;
        std::array<std::string, 6> cubemap_textures;
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
        std::optional<std::string> light_texture = std::nullopt;
    };
	struct DecalParameters
	{
		std::string name = "Decal";
		std::string albedo_texture_path;
		std::string normal_texture_path;
		float rotation = 0.0f;
		float size = 50.0f;
		bool modify_gbuffer_normals = false;
		Vector3 position;
		Vector3 normal;
	};

    class GfxDevice;
 
	class EntityLoader
	{
		ADRIA_NODISCARD std::vector<entt::entity> LoadGrid(GridParameters const&);
		ADRIA_NODISCARD std::vector<entt::entity> LoadObjMesh(std::string const&);
	public:
        
        EntityLoader(entt::registry& reg, GfxDevice* device);
		~EntityLoader();

		ADRIA_MAYBE_UNUSED entt::entity LoadSkybox(SkyboxParameters const&);
        ADRIA_MAYBE_UNUSED entt::entity LoadLight(LightParameters const&);
		ADRIA_MAYBE_UNUSED std::vector<entt::entity> LoadOcean(OceanParameters const&);
		ADRIA_MAYBE_UNUSED entt::entity LoadDecal(DecalParameters const&);
		ADRIA_MAYBE_UNUSED entt::entity ImportModel_GLTF(ModelParameters const&);
	private:
        entt::registry& reg;
        GfxDevice* gfx;
	};
}

