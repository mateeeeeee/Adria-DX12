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
		float32 tile_size_x;
		float32 tile_size_z;
		float32 texture_scale_x;
		float32 texture_scale_z;
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
	struct EmitterParameters
	{
		std::string name = "Emitter";
		float32 position[3] = { 50.0f, 10.0f, 0.0f };
		float32 velocity[3] = { 0.0f, 7.0f, 0.0f };
		float32 position_variance[3] = { 4.0f, 0.0f, 4.0f };
		float32 velocity_variance = { 0.6f };
		float32 lifespan = 50.0f;
		float32 start_size = 22.0f;
		float32 end_size = 5.0f;
		float32 mass = 0.0003f;
		float32 particles_per_second = 100.0f;
		std::wstring texture_path = L"Resources/Textures/Particles/fire.png";
		bool blend = true;
		bool collisions = false;
		int32 collision_thickness = 40;
		bool sort = false;
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
		float32 rotation = 0.0f;
		float32 size = 50.0f;
		bool modify_gbuffer_normals = false;
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT4 normal;
	};

    class TextureManager;
    class GraphicsDevice;
    
	class EntityLoader
	{
		[[nodiscard]] std::vector<tecs::entity> LoadGrid(GridParameters const&);

		[[nodiscard]] std::vector<tecs::entity> LoadObjMesh(std::string const&);

	public:
        
        EntityLoader(tecs::registry& reg, GraphicsDevice* device, TextureManager& texture_manager);

		[[maybe_unused]] std::vector<tecs::entity> LoadGLTFModel(ModelParameters const&);

		[[maybe_unused]] tecs::entity LoadSkybox(SkyboxParameters const&);

        [[maybe_unused]] tecs::entity LoadLight(LightParameters const&);

		[[maybe_unused]] std::vector<tecs::entity> LoadOcean(OceanParameters const&);

		[[maybe_unused]] tecs::entity LoadEmitter(EmitterParameters const&);

		[[maybe_unused]] tecs::entity LoadDecal(DecalParameters const&);

	private:
        tecs::registry& reg;
        GraphicsDevice* gfx;
        TextureManager& texture_manager;
	};
}

