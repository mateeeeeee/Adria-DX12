#pragma once
#include "RendererSettings.h"
#include "ViewportData.h"
#include "ShaderStructs.h"
#include "Postprocessor.h"
#include "GBufferPass.h"
#include "AmbientPass.h"
#include "SkyPass.h"
#include "DeferredLightingPass.h"
#include "VolumetricLightingPass.h"
#include "TiledDeferredLightingPass.h"
#include "ClusteredDeferredLightingPass.h"
#include "ToneMapPass.h"
#include "FXAAPass.h"
#include "SSAOPass.h"
#include "HBAOPass.h"
#include "CopyToTexturePass.h"
#include "AddTexturesPass.h"
#include "PickingPass.h"
#include "DecalsPass.h"
#include "AABBPass.h"
#include "OceanRenderer.h"
#include "AccelerationStructure.h"
#include "RayTracedShadowsPass.h"
#include "RayTracedAmbientOcclusionPass.h"
#include "RayTracedReflectionsPass.h"
#include "PathTracingPass.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxConstantBuffer.h"
#include "Graphics/GPUProfiler.h"
#include "Utilities/CPUProfiler.h" 
#include "RenderGraph/RenderGraphResourcePool.h"

namespace adria
{
	class Camera;
	class GfxBuffer;
	class GfxCommandList;
	class GfxTexture;
	struct Light;


	class Renderer
	{
	public:

		Renderer(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height);
		~Renderer();

		void NewFrame(Camera const* camera);
		void Update(float dt);
		void Render(RendererSettings const&);
		void SetViewportData(ViewportData const& vp);

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
		void OnRightMouseClicked(int32 x, int32 y);

		GfxDescriptor GetFinalTextureSRV() const 
		{ 
			return final_texture_srv;
		}
		
		PickingData const& GetPickingData() const { return picking_data; }
		bool IsRayTracingSupported() const { return is_ray_tracing_supported; }

		uint32 GetWidth() const { return width; }
		uint32 GetHeight() const { return height; }

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		RGResourcePool resource_pool;

		CPUProfiler cpu_profiler;
		RendererSettings renderer_settings;
		Camera const* camera;

		uint32 const backbuffer_count;
		uint32 backbuffer_index;
		uint32 width;
		uint32 height;

		//resources
		std::unique_ptr<GfxTexture> final_texture;
		GfxDescriptor final_texture_srv;

		//Persistent constant buffers
		GfxConstantBuffer<FrameCBuffer> frame_cbuffer;

		//lights and shadows
		std::unique_ptr<GfxBuffer>  lights_buffer;
		GfxDescriptor				lights_buffer_srvs[GFX_BACKBUFFER_COUNT];
		std::unique_ptr<GfxBuffer>  light_matrices_buffer;
		GfxDescriptor				light_matrices_buffer_srvs[GFX_BACKBUFFER_COUNT];
		HashMap<size_t, std::vector<std::unique_ptr<GfxTexture>>> light_shadow_maps;
		HashMap<size_t, std::vector<GfxDescriptor>> light_shadow_map_srvs;
		HashMap<size_t, std::vector<GfxDescriptor>> light_shadow_map_dsvs;
		HashMap<size_t, std::unique_ptr<GfxTexture>> light_mask_textures;
		HashMap<size_t, GfxDescriptor> light_mask_texture_srvs;
		HashMap<size_t, GfxDescriptor> light_mask_texture_uavs;

		GfxDescriptor		 light_array_srv;
		GfxDescriptor		 light_matrices_srv;
		GfxDescriptor        env_map_srv;

		//passes
		GBufferPass  gbuffer_pass;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		AmbientPass  ambient_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;
		SkyPass		 sky_pass;
		RayTracedShadowsPass ray_traced_shadows_pass;
		RayTracedAmbientOcclusionPass rtao_pass;
		RayTracedReflectionsPass rtr_pass;
		DeferredLightingPass deferred_lighting_pass;
		VolumetricLightingPass volumetric_lighting_pass;
		TiledDeferredLightingPass tiled_deferred_lighting_pass;
		ClusteredDeferredLightingPass clustered_deferred_lighting_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		PickingPass picking_pass;
		DecalsPass decals_pass;
		OceanRenderer  ocean_renderer;
		AABBPass aabb_pass;
		Postprocessor postprocessor;
		PathTracingPass path_tracer;

		//ray tracing
		AccelerationStructure accel_structure;
		GfxDescriptor tlas_srv;
		bool is_ray_tracing_supported;
		std::unique_ptr<GfxBuffer> global_vb = nullptr;
		std::unique_ptr<GfxBuffer> global_ib = nullptr;
		std::unique_ptr<GfxBuffer> geo_buffer = nullptr;

		//misc
		uint32			         volumetric_lights = 0;
		float					 cascades_split_lambda = 0.5f;
		bool				     transparent_shadows = false;
		std::array<float, 4>	 split_distances;
		float					 wind_dir[3] = { 1.0f, 0.0f, 1.0f };
		float					 wind_speed = 10.0f;
		DirectX::XMFLOAT3		 sun_direction;
		bool update_picking_data = false;
		PickingData picking_data;
		ViewportData viewport_data;

	private:
		void CheckDeviceCapabilities();
		void CreateSizeDependentResources();
		void CreateGlobalBuffers();

		void UpdateEnvironmentMap();
		void MiscGUI();
		void SetupShadows();
		void UpdateLights();
		void UpdateFrameConstantBuffer(float dt);
		void CameraFrustumCulling();

		void Render_Deferred(RenderGraph& rg);
		void Render_PathTracing(RenderGraph& rg);

		void ShadowMapPass_Common(GfxDevice* gfx, GfxCommandList* cmd_list, bool transparent, size_t light_index, size_t shadow_map_index = 0);
		void AddShadowMapPasses(RenderGraph& rg);
		void AddRayTracingShadowPasses(RenderGraph& rg);
		void CopyToBackbuffer(RenderGraph& rg);
		void ResolveToFinalTexture(RenderGraph& rg);
	};
}