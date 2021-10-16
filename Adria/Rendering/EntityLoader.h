#pragma once
#include <DirectXMath.h>
#include <optional>
#include <array>
#include <vector>
#include <string>
#include "Components.h"
#include "../Core/Definitions.h"
#include "../tecs/registry.h"
#include "../Math/ComputeNormals.h"

struct ID3D12Device;

namespace adria
{
    class Heightmap;

    struct model_parameters_t
    {
        std::string model_path = "";
        std::string textures_path = "";
		bool merge_meshes = true;
		f32 model_scale = 1.0f;
    };
    struct skybox_parameters_t
    {
        std::optional<std::wstring> cubemap;
        std::array<std::wstring, 6> cubemap_textures;
    };
	struct grid_parameters_t
	{
		u64 tile_count_x;
		u64 tile_count_z;
		f32 tile_size_x;
		f32 tile_size_z;
		f32 texture_scale_x;
		f32 texture_scale_z;
		u64 chunk_count_x;
		u64 chunk_count_z;
		bool split_to_chunks = false;
		ENormalCalculation normal_type = ENormalCalculation::None;
		std::unique_ptr<Heightmap> heightmap = nullptr;
	};
	struct ocean_parameters_t
	{
		grid_parameters_t ocean_grid;
	};

    enum class ELightMesh
    {
        NoMesh,
        Quad,
        Sphere
    };

    struct light_parameters_t
    {
        Light light_data;
        ELightMesh mesh_type = ELightMesh::NoMesh;
        u32 mesh_size = 0u;
        std::optional<std::wstring> light_texture = std::nullopt;
    };

    class TextureManager;
    class GraphicsCoreDX12;
    
	class EntityLoader
	{
		[[nodiscard]] std::vector<tecs::entity> LoadGrid(grid_parameters_t const&);

		[[nodiscard]] std::vector<tecs::entity> LoadObjMesh(std::string const&);

	public:
        
        EntityLoader(tecs::registry& reg, GraphicsCoreDX12* device, TextureManager& texture_manager);

		[[maybe_unused]] std::vector<tecs::entity> LoadGLTFModel(model_parameters_t const&);

		[[maybe_unused]] tecs::entity LoadSkybox(skybox_parameters_t const&);

        [[maybe_unused]] tecs::entity LoadLight(light_parameters_t const&);

		[[maybe_unused]] std::vector<tecs::entity> LoadOcean(ocean_parameters_t const&);

	private:
        tecs::registry& reg;
        GraphicsCoreDX12* gfx;
        TextureManager& texture_manager;
	};
}

