#pragma once
#include "RendererSettings.h"
#include "ViewportData.h"
#include "ShaderStructs.h"
#include "Postprocessor.h"
#include "GBufferPass.h"
#include "GPUDrivenGBufferPass.h"
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
#include "DebugPass.h"
#include "OceanRenderer.h"
#include "AccelerationStructure.h"
#include "ShadowRenderer.h"
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

		PickingData const& GetPickingData() const { return picking_data; }

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

		std::unique_ptr<GfxTexture> final_texture;
		GfxConstantBuffer<FrameCBuffer> frame_cbuffer;

		//scene buffers
		enum SceneBufferType { SceneBuffer_Light, SceneBuffer_Mesh, SceneBuffer_Material, SceneBuffer_Instance, SceneBuffer_Count };
		struct SceneBuffer
		{
			std::unique_ptr<GfxBuffer>  buffer;
			GfxDescriptor				buffer_srv;
			GfxDescriptor				buffer_srv_gpu;
		};
		std::array<SceneBuffer, SceneBuffer_Count> scene_buffers;

		//passes
		GBufferPass  gbuffer_pass;
		GPUDrivenGBufferPass gpu_driven_renderer;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		AmbientPass  ambient_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;
		SkyPass		 sky_pass;
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
		ShadowRenderer shadow_renderer;
		DebugPass aabb_pass;
		Postprocessor postprocessor;
		PathTracingPass path_tracer;

		//ray tracing
		bool ray_tracing_supported = false;
		AccelerationStructure accel_structure;
		GfxDescriptor tlas_srv;

		//picking
		bool update_picking_data = false;
		PickingData picking_data;

		//misc
		uint32			         volumetric_lights = 0;
		float					 wind_dir[3] = { 1.0f, 0.0f, 1.0f };
		float					 wind_speed = 10.0f;
		DirectX::XMFLOAT3		 sun_direction;
		ViewportData			 viewport_data;

	private:
		void CreateSizeDependentResources();
		void CreateAS();

		void MiscGUI();
		void UpdateSceneBuffers();
		void UpdateFrameConstants(float dt);
		void CameraFrustumCulling();

		void Render_Deferred(RenderGraph& rg);
		void Render_PathTracing(RenderGraph& rg);

		void CopyToBackbuffer(RenderGraph& rg);
		void ResolveToFinalTexture(RenderGraph& rg);
	};
}