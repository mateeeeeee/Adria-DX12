#pragma once
#include "RendererSettings.h"
#include "ViewportData.h"
#include "ConstantBuffers.h"
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
#include "RayTracer.h"
#include "../Graphics/ShaderCompiler.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/GPUProfiler.h"
#include "../Utilities/CPUProfiler.h" 
#include "../RenderGraph/RenderGraphResourcePool.h"

namespace adria
{
	class Camera;
	class Buffer;
	class Texture;
	struct Light;

	class Renderer
	{
	public:

		Renderer(entt::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
		~Renderer();

		void NewFrame(Camera const* camera);
		void Update(float dt);
		void Render(RendererSettings const&);

		void SetViewportData(ViewportData const& vp)
		{
			viewport_data = vp;
		}

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
		void OnRightMouseClicked(int32 x, int32 y);

		Texture const* GetFinalTexture() const 
		{ 
			return final_texture.get(); 
		}
		TextureManager& GetTextureManager();
		PickingData const& GetPickingData() const { return picking_data; }
		bool IsRayTracingSupported() const { return ray_tracer.IsSupported(); }
	private:
		entt::registry& reg;
		GraphicsDevice* gfx;
		TextureManager texture_manager;
		RGResourcePool resource_pool;

		CPUProfiler cpu_profiler;
		RendererSettings renderer_settings;
		Camera const* camera;

		uint32 const backbuffer_count;
		uint32 backbuffer_index;
		uint32 width;
		uint32 height;

		//resources
		std::unique_ptr<Texture> final_texture;
		std::unique_ptr<Texture> white_default_texture;

		//Persistent constant buffers
		ConstantBuffer<FrameCBuffer> frame_cbuffer;

		//lights and shadows
		std::unique_ptr<Buffer>  lights_buffer;
		std::unique_ptr<Buffer>  light_matrices_buffer;
		HashMap<size_t, std::vector<std::unique_ptr<Texture>>> light_shadow_maps;
		HashMap<size_t, std::unique_ptr<Texture>> light_mask_textures;
		DescriptorHandle		 light_array_srv; 
		DescriptorHandle		 light_matrices_srv; 
		DescriptorHandle         env_map_srv;
		//passes
		GBufferPass  gbuffer_pass;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		AmbientPass  ambient_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;
		SkyPass		 sky_pass;
		DeferredLightingPass deferred_lighting_pass;
		VolumetricLightingPass volumetric_lighting_pass;
		TiledDeferredLightingPass tiled_deferred_lighting_pass;
		ClusteredDeferredLightingPass clustered_deferred_lighting_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		PickingPass picking_pass;
		DecalsPass decals_pass;
		OceanRenderer  ocean_renderer;
		RayTracer ray_tracer;
		AABBPass aabb_pass;
		Postprocessor postprocessor;

		ViewportData viewport_data;

		//misc
		uint32			         volumetric_lights = 0;
		float					 cascades_split_lambda = 0.5f;
		bool				     transparent_shadows = false;
		std::array<float, 4>	 split_distances;

		float wind_dir[3] = { 1.0f, 0.0f, 1.0f };
		float wind_speed = 10.0f;
		DirectX::XMFLOAT3 sun_direction;

		std::unique_ptr<DescriptorHeap> null_heap;

		bool update_picking_data = false;
		PickingData picking_data;

	private:
		void CreateNullHeap();
		void CreateSizeDependentResources();

		void UpdateEnvironmentMap();
		void MiscGUI();
		void SetupShadows();
		void UpdateLights();
		void UpdateFrameConstantBuffer(float dt);
		void CameraFrustumCulling();

		void Render_Deferred(RenderGraph& rg);
		void Render_PathTracing(RenderGraph& rg);

		void ShadowMapPass_Common(GraphicsDevice* gfx, ID3D12GraphicsCommandList4* cmd_list, bool transparent, size_t light_index, size_t shadow_map_index = 0);
		void AddShadowMapPasses(RenderGraph& rg);
		void AddRayTracingShadowPasses(RenderGraph& rg);
		void CopyToBackbuffer(RenderGraph& rg);
		void ResolveToFinalTexture(RenderGraph& rg);
	};
}