#pragma once
#include "ViewportData.h"
#include "ShaderStructs.h"
#include "PostProcessor.h"
#include "GBufferPass.h"
#include "GPUDrivenGBufferPass.h"
#include "SkyPass.h"
#include "DeferredLightingPass.h"
#include "RayMarchedVolumetricFogPass.h"
#include "FogVolumesPass.h"
#include "TiledDeferredLightingPass.h"
#include "ClusteredDeferredLightingPass.h"
#include "DDGIPass.h"
#include "ReSTIR_DI.h"
#include "GPUDebugPrinter.h"
#include "HelperPasses.h"
#include "PickingPass.h"
#include "DecalsPass.h"
#include "RainPass.h"
#include "OceanRenderer.h"
#include "AccelerationStructure.h"
#include "ShadowRenderer.h"
#include "PathTracingPass.h"
#include "TransparentPass.h"
#include "VolumetricFogManager.h"
#include "RendererDebugViewPass.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxConstantBuffer.h"
#include "RenderGraph/RenderGraphResourcePool.h"

namespace adria
{
	class Camera;
	class GfxBuffer;
	class GfxCommandList;
	class GfxTexture;
	struct Light;

	enum class LightingPath : Uint8
	{
		Deferred,
		TiledDeferred,
		ClusteredDeferred,
		PathTracing
	};

	class Renderer
	{
	public:

		Renderer(entt::registry& reg, GfxDevice* gfx, Uint32 width, Uint32 height);
		~Renderer();

		void NewFrame(Camera const* camera);
		void Update(Float dt);
		void Render();

		void OnResize(Uint32 w, Uint32 h);
		void OnRenderResolutionChanged(Uint32 w, Uint32 h);
		void OnSceneInitialized();
		void OnRightMouseClicked(Int32 x, Int32 y);
		void OnTakeScreenshot(Char const*);
		void OnLightChanged();

		PickingData const& GetPickingData() const { return picking_data; }
		Vector2u GetDisplayResolution() const { return Vector2u(display_width, display_height); }

		void SetLightingPath(LightingPath path);
		LightingPath GetLightingPath() const { return lighting_path; }
		void SetDebugView(RendererDebugView debug_view);
		RendererDebugView GetDebugView() const { return renderer_debug_view_pass.GetDebugView(); }
		void SetViewportData(ViewportData const& vp);

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		RGResourcePool resource_pool;

		Camera const* camera;
		Vector2 camera_jitter;

		Uint32 const backbuffer_count;
		Uint32 backbuffer_index;
		Uint32 display_width;
		Uint32 display_height;
		Uint32 render_width;
		Uint32 render_height;

		std::unique_ptr<GfxTexture> final_texture;

		FrameCBuffer frame_cbuf_data{};
		GfxConstantBuffer<FrameCBuffer> frame_cbuffer;

		//scene buffers
		enum SceneBufferType 
		{ 
			SceneBuffer_Light, 
			SceneBuffer_Mesh, 
			SceneBuffer_Material, 
			SceneBuffer_Instance, 
			SceneBuffer_Count 
		};
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
		SkyPass		 sky_pass;
		DeferredLightingPass deferred_lighting_pass;
		
		TiledDeferredLightingPass tiled_deferred_lighting_pass;
		ClusteredDeferredLightingPass clustered_deferred_lighting_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		PickingPass picking_pass;
		DecalsPass decals_pass;
		RainPass rain_pass;
		OceanRenderer  ocean_renderer;
		ShadowRenderer shadow_renderer;
		PostProcessor postprocessor;
		DDGIPass		  ddgi;
		ReSTIR_DI		  restir_di;
		PathTracingPass path_tracer;
		RendererDebugViewPass renderer_debug_view_pass;
		GPUDebugPrinter gpu_debug_printer;
		TransparentPass transparent_pass;
		VolumetricFogManager volumetric_fog_manager;

		//ray tracing
		Bool ray_tracing_supported = false;
		AccelerationStructure accel_structure;
		GfxDescriptor tlas_srv;

		//picking
		Bool update_picking_data = false;
		PickingData picking_data;

		LightingPath		lighting_path = LightingPath::Deferred;

		std::unique_ptr<GfxTexture> overdraw_texture = nullptr;
		GfxDescriptor				overdraw_texture_uav;
		GfxDescriptor				overdraw_texture_uav_gpu;

		TextureHandle		 sheenE_texture = INVALID_TEXTURE_HANDLE;

		//weather
		Float					 ambient_color[3] = { 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f };
		Float					 wind_dir[3] = { 1.0f, 0.0f, 1.0f };
		Float					 wind_speed = 10.0f;
		Vector3					 sun_direction;

		//screenshot
		Bool						take_screenshot = false;
		std::string					screenshot_name = "";
		GfxFence					screenshot_fence;
		Uint64						screenshot_fence_value = 1;
		std::unique_ptr<GfxBuffer>  screenshot_buffer;

		//misc
		ViewportData			 viewport_data;

	private:
		void RegisterEventListeners();
		void CreateDisplaySizeDependentResources();
		void CreateRenderSizeDependentResources();
		void CreateAS();

		void GUI();
		void UpdateSceneBuffers();
		void UpdateFrameConstants(Float dt);
		void CameraFrustumCulling();

		void RenderImpl(RenderGraph& rg);
		void Render_Deferred(RenderGraph& rg);
		void Render_PathTracing(RenderGraph& rg);

		void ClearTriangleOverdrawTexture(RenderGraph& rg);
		void CopyToBackbuffer(RenderGraph& rg);
		void TakeScreenshot(RenderGraph& rg);
	};
}