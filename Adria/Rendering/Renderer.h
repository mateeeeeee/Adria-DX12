#pragma once
#include "RendererSettings.h"
#include "ViewportData.h"
#include "ConstantBuffers.h"
#include "Postprocessor.h"
#include "GBufferPass.h"
#include "AmbientPass.h"
#include "SkyPass.h"
#include "DeferredLightingPass.h"
#include "TiledLightingPass.h"
#include "ClusteredLightingPass.h"
#include "ShadowPass.h"
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
	namespace tecs
	{
		class registry;
	}
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
		std::unique_ptr<Buffer>  lights_buffer;
		std::unique_ptr<Buffer>  lights_projections_buffer;
		DescriptorHandle		 light_array_srv;
		DirectX::XMFLOAT3        sun_direction;

		//Persistent constant buffers
		ConstantBuffer<FrameCBuffer> frame_cbuffer;
		ConstantBuffer<OldFrameCBuffer> old_frame_cbuffer;
		ConstantBuffer<ComputeCBuffer> compute_cbuffer;

		//misc
		std::unique_ptr<DescriptorHeap> null_heap;
		bool update_picking_data = false;
		PickingData picking_data;

		std::unique_ptr<Texture> env_texture;
		std::unique_ptr<Texture> irmap_texture;
		std::unique_ptr<Texture> brdf_lut_texture;
		bool ibl_generated = false;

		//passes
		GBufferPass  gbuffer_pass;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		AmbientPass  ambient_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;
		SkyPass		 sky_pass;
		DeferredLightingPass deferred_lighting_pass;
		TiledLightingPass tiled_lighting_pass;
		ClusteredLightingPass clustered_lighting_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		ShadowPass    shadow_pass;
		PickingPass picking_pass;
		DecalsPass decals_pass;
		OceanRenderer  ocean_renderer;
		RayTracer ray_tracer;
		AABBPass aabb_pass;
		Postprocessor postprocessor;
		
		ViewportData viewport_data;
	private:
		void CreateNullHeap();
		void CreateSizeDependentResources();

		void UpdateLights();
		void UpdatePersistentConstantBuffers(float dt);
		void CameraFrustumCulling();

		void CopyToBackbuffer(RenderGraph& rg);
		void ResolveToFinalTexture(RenderGraph& rg);
	};
}