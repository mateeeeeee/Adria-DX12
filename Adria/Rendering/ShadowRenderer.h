#pragma once
#include <array>
#include <variant>
#include "RayTracedShadowsPass.h"
#include "Graphics/GfxMacros.h"
#include "Graphics/GfxDescriptor.h"
#include "Graphics/GfxPipelineStatePermutationsFwd.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class GfxBuffer;
	class GfxTexture;
	class RenderGraph;
	class Camera;
	struct FrameCBuffer;
	enum class LightType : Sint32;

	struct BoundingObject
	{
		enum Type
		{
			Box,
			Frustum
		} type = Box;

		BoundingObject(BoundingBox const& box) : data(box) {}
		BoundingObject(BoundingFrustum const& frustum) : data(frustum) {}

		BoundingBox const& GetBox() const
		{
			return std::get<Box>(data);
		}
		BoundingFrustum const& GetFrustum() const
		{
			return std::get<Frustum>(data);
		}
		
		std::variant<BoundingBox, BoundingFrustum> data;
	};

	DECLARE_EVENT(ShadowTextureRenderedEvent, ShadowRenderer, RGResourceName)

	class ShadowRenderer
	{
		static constexpr Uint32 SHADOW_MAP_SIZE = 2048;
		static constexpr Uint32 SHADOW_CUBE_SIZE = 512;
		static constexpr Uint32 SHADOW_CASCADE_MAP_SIZE = 1024;
		static constexpr Uint32 SHADOW_CASCADE_COUNT = 4;

	public:
		ShadowRenderer(entt::registry& reg, GfxDevice* gfx, Uint32 width, Uint32 height);
		~ShadowRenderer();

		void OnResize(Uint32 w, Uint32 h)
		{
			if (width != w || height != h)
			{
				width = w, height = h;
				ray_traced_shadows_pass.OnResize(w, h);
				light_mask_textures.clear();
			}
		}
		void SetupShadows(Camera const* camera);

		void AddShadowMapPasses(RenderGraph& rg);
		void AddRayTracingShadowPasses(RenderGraph& rg);

		void FillFrameCBuffer(FrameCBuffer& frame_cbuffer);

		ShadowTextureRenderedEvent& GetShadowTextureRenderedEvent() { return shadow_rendered_event; }

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		Uint32 width;
		Uint32 height;
		RayTracedShadowsPass ray_traced_shadows_pass;
		std::unique_ptr<GfxGraphicsPipelineStatePermutations> shadow_psos;

		std::unique_ptr<GfxBuffer>  light_matrices_buffer;
		GfxDescriptor				light_matrices_buffer_srvs[GFX_BACKBUFFER_COUNT];
		std::unordered_map<Uint64, std::vector<std::unique_ptr<GfxTexture>>> light_shadow_maps;
		std::unordered_map<Uint64, std::vector<GfxDescriptor>> light_shadow_map_srvs;
		std::unordered_map<Uint64, std::vector<GfxDescriptor>> light_shadow_map_dsvs;
		std::unordered_map<Uint64, std::unique_ptr<GfxTexture>> light_mask_textures;
		std::unordered_map<Uint64, GfxDescriptor> light_mask_texture_srvs;
		std::unordered_map<Uint64, GfxDescriptor> light_mask_texture_uavs;
		Sint32						   light_matrices_gpu_index = -1;

		std::vector<BoundingObject>						bounding_objects;
		std::array<Float, SHADOW_CASCADE_COUNT>		    split_distances{};

		ShadowTextureRenderedEvent shadow_rendered_event;

	private:
		void CreatePSOs();
		void ShadowMapPass_Common(GfxCommandList* cmd_list, LightType light_type, Uint64 light_index, Uint64 matrix_index, Uint64 matrix_offset);
		static std::array<Matrix, SHADOW_CASCADE_COUNT> RecalculateProjectionMatrices(Camera const& camera, Float split_lambda, std::array<Float, SHADOW_CASCADE_COUNT>& split_distances);
	};
}