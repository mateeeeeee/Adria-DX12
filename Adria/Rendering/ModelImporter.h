#pragma once
#pragma comment(lib, "assimp-vc142-mt.lib")
#include <DirectXMath.h>
#include <optional>
#include <array>
#include <vector>
#include <string>
#include "Components.h"
#include "../Core/Definitions.h"
#include "../tecs/registry.h"

struct ID3D12Device;

namespace adria
{
    struct model_parameters_t
    {
        std::string model_path = "";
        std::string textures_path = "";
        DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();
    };

    struct skybox_parameters_t
    {
        std::optional<std::wstring> cubemap;
        std::array<std::wstring, 6> cubemap_textures;
    };

    enum class LightMesh
    {
        eNoMesh,
        eQuad,
        eSphere
    };

    struct light_parameters_t
    {
        Light light_data;
        LightMesh mesh_type = LightMesh::eNoMesh;
        u32 mesh_size = 0u;
        std::optional<std::wstring> light_texture = std::nullopt;
    };





    class TextureManager;
    class GraphicsCoreDX12;
    
	class ModelImporter
	{

	public:
        
        ModelImporter(tecs::registry& reg, GraphicsCoreDX12* device, TextureManager& texture_manager);

        [[maybe_unused]] tecs::entity LoadSkybox(skybox_parameters_t const&);

        [[maybe_unused]] std::vector<tecs::entity> LoadGLTFModel(model_parameters_t const&);

        [[maybe_unused]] tecs::entity LoadLight(light_parameters_t const&);

        void LoadModelMesh(tecs::entity, model_parameters_t const&);

	private:
        tecs::registry& reg;
        GraphicsCoreDX12* gfx;
        TextureManager& texture_manager;
	};
}

