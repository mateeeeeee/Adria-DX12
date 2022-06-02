#pragma once
#include <optional>
#include <DirectXCollision.h>
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceRef.h"

namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class Camera;
	class TextureManager;
	struct Light;
	enum class ELightType : int32;

	struct ShadowPassData
	{
		RGTextureSRVRef depth_map_srv;
		RGTextureDSVRef depth_map_dsv;
	};

	struct ShadowBlackboardData
	{
		D3D12_GPU_VIRTUAL_ADDRESS shadow_allocation;
	};

	class ShadowPass
	{
	public:

		static constexpr uint32 SHADOW_MAP_SIZE = 2048;
		static constexpr uint32 SHADOW_CUBE_SIZE = 512;
		static constexpr uint32 SHADOW_CASCADE_MAP_SIZE = 1024;
		static constexpr uint32 CASCADE_COUNT = 3;
	public:
		ShadowPass(tecs::registry& reg, TextureManager& texture_manager);
		void SetCamera(Camera const* camera);
		ShadowPassData const& AddPass(RenderGraph& rg, Light const& light);
	private:
		tecs::registry& reg;
		TextureManager& texture_manager;
		Camera const* camera;
	private:
		
		ShadowPassData const& ShadowMapPass_Directional(RenderGraph& rg, Light const& light, std::optional<DirectX::BoundingSphere> const& scene_bounding_sphere = std::nullopt);
		ShadowPassData const& ShadowMapPass_DirectionalCascades(RenderGraph& rg, Light const& light);
		ShadowPassData const& ShadowMapPass_Point(RenderGraph& rg, Light const& light);
		ShadowPassData const& ShadowMapPass_Spot(RenderGraph& rg, Light const& light);

		void ShadowMapPass_Common(GraphicsDevice* gfx, ID3D12GraphicsCommandList* cmd_list, 
					D3D12_GPU_VIRTUAL_ADDRESS allocation_address, bool transparent);

		void LightFrustumCulling(ELightType type, std::optional<DirectX::BoundingBox> const& light_bounding_box, std::optional<DirectX::BoundingFrustum> const& light_bounding_frustum);
	};
}