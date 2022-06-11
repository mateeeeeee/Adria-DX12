#pragma once
#include <optional>
#include <d3d12.h>
#include <DirectXCollision.h>
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../tecs/entity.h"

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
	class GraphicsDevice;
	enum class ELightType : int32;

	class ShadowPass
	{
	public:
		static constexpr uint32 SHADOW_MAP_SIZE = 2048;
		static constexpr uint32 SHADOW_CUBE_SIZE = 512;
		static constexpr uint32 SHADOW_CASCADE_MAP_SIZE = 1024;
		static constexpr uint32 SHADOW_CASCADE_COUNT = 3;

	public:
		ShadowPass(tecs::registry& reg, TextureManager& texture_manager);
		void SetCamera(Camera const* camera);
		void AddPass(RenderGraph& rg, Light const& light, size_t light_id);

	private:
		tecs::registry& reg;
		TextureManager& texture_manager;
		Camera const* camera;

	private:
		void ShadowMapPass_Directional(RenderGraph& rg, Light const& light, size_t light_id, std::optional<DirectX::BoundingSphere> const& scene_bounding_sphere = std::nullopt);
		void ShadowMapPass_DirectionalCascades(RenderGraph& rg, Light const& light, size_t light_id);
		void ShadowMapPass_Point(RenderGraph& rg, Light const& light, size_t light_id);
		void ShadowMapPass_Spot(RenderGraph& rg, Light const& light, size_t light_id);

		void ShadowMapPass_Common(GraphicsDevice* gfx, ID3D12GraphicsCommandList4* cmd_list,
			D3D12_GPU_VIRTUAL_ADDRESS allocation_address, bool transparent);

		void LightFrustumCulling(ELightType type, std::optional<DirectX::BoundingBox> const& light_bounding_box, std::optional<DirectX::BoundingFrustum> const& light_bounding_frustum);
	};
}