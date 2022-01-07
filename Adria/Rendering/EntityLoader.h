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
#include "../Utilities/Heightmap.h"

struct ID3D12Device;

namespace adria
{


	enum class ELightMesh
	{
		NoMesh,
		Quad,
		Sphere
	};

    struct model_parameters_t
    {
        std::string model_path = "";
        std::string textures_path = "";
		bool merge_meshes = true;
		F32 model_scale = 1.0f;
    };
    struct skybox_parameters_t
    {
        std::optional<std::wstring> cubemap;
        std::array<std::wstring, 6> cubemap_textures;
    };
	struct grid_parameters_t
	{
		U64 tile_count_x;
		U64 tile_count_z;
		F32 tile_size_x;
		F32 tile_size_z;
		F32 texture_scale_x;
		F32 texture_scale_z;
		U64 chunk_count_x;
		U64 chunk_count_z;
		bool split_to_chunks = false;
		ENormalCalculation normal_type = ENormalCalculation::None;
		std::unique_ptr<Heightmap> heightmap = nullptr;
	};
	struct ocean_parameters_t
	{
		grid_parameters_t ocean_grid;
	};
	struct emitter_parameters_t
	{
		std::string name = "Emitter";
		F32 position[3] = { 50.0f, 10.0f, 0.0f };
		F32 velocity[3] = { 0.0f, 7.0f, 0.0f };
		F32 position_variance[3] = { 4.0f, 0.0f, 4.0f };
		F32 velocity_variance = { 0.6f };
		F32 lifespan = 50.0f;
		F32 start_size = 22.0f;
		F32 end_size = 5.0f;
		F32 mass = 0.0003f;
		F32 particles_per_second = 100.0f;
		std::wstring texture_path = L"Resources/Textures/Particles/fire.png";
		bool blend = true;
		bool collisions = false;
		I32 collision_thickness = 40;
		bool sort = false;
	};
    struct light_parameters_t
    {
        Light light_data;
        ELightMesh mesh_type = ELightMesh::NoMesh;
        U32 mesh_size = 0u;
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

		[[maybe_unused]] tecs::entity LoadEmitter(emitter_parameters_t const&);

	private:
        tecs::registry& reg;
        GraphicsCoreDX12* gfx;
        TextureManager& texture_manager;
	};
}

