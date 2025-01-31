#pragma once
#include "ViewportData.h"
#include "ShaderStructs.h"
#include "PostProcessor.h"
#include "GBufferPass.h"
#include "GPUDrivenGBufferPass.h"
#include "SkyPass.h"
#include "DeferredLightingPass.h"
#include "VolumetricLightingPass.h"
#include "VolumetricFogPass.h"
#include "TiledDeferredLightingPass.h"
#include "ClusteredDeferredLightingPass.h"
#include "DDGIPass.h"
#include "GPUDebugPrinter.h"
#include "HelperPasses.h"
#include "PickingPass.h"
#include "DecalsPass.h"
#include "RainPass.h"
#include "OceanRenderer.h"
#include "AccelerationStructure.h"
#include "ShadowRenderer.h"
#include "PathTracingPass.h"
#include "RendererOutputPass.h"
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

	enum class LightingPathType : Uint8
	{
		Deferred,
		TiledDeferred,
		ClusteredDeferred,
		PathTracing
	};

	class Renderer
	{
		enum class VolumetricPathType : Uint8
		{
			None,
			Raymarching,
			FogVolume
		};

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

		RendererOutput GetRendererOutput() const { return renderer_output; }
		LightingPathType GetLightingPath() const { return lighting_path; }
		void SetRendererOutput(RendererOutput type)
		{
			renderer_output = type;
			gbuffer_pass.OnRendererOutputChanged(type);
			gpu_driven_renderer.OnRendererOutputChanged(type);
		}
		void SetLightingPath(LightingPathType path);
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
		VolumetricLightingPass volumetric_lighting_pass;
		VolumetricFogPass volumetric_fog_pass;
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
		PathTracingPass path_tracer;
		RendererOutputPass renderer_output_pass;
		GPUDebugPrinter gpu_debug_printer;

		//ray tracing
		Bool ray_tracing_supported = false;
		AccelerationStructure accel_structure;
		GfxDescriptor tlas_srv;

		//picking
		Bool update_picking_data = false;
		PickingData picking_data;

		LightingPathType	 lighting_path = LightingPathType::Deferred;
		RendererOutput		 renderer_output = RendererOutput::Final;

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

		//volumetric
		Uint32			         volumetric_lights = 0;
		VolumetricPathType		 volumetric_path = VolumetricPathType::Raymarching;
		//misc
		ViewportData			 viewport_data;

	private:
		void CreateDisplaySizeDependentResources();
		void CreateRenderSizeDependentResources();
		void CreateAS();

		void GUI();
		void UpdateSceneBuffers();
		void UpdateFrameConstants(Float dt);
		void CameraFrustumCulling();

		void Render_Deferred(RenderGraph& rg);
		void Render_PathTracing(RenderGraph& rg);

		void ClearTriangleOverdrawTexture(RenderGraph& rg);
		void CopyToBackbuffer(RenderGraph& rg);
		void TakeScreenshot(RenderGraph& rg);
	};
}