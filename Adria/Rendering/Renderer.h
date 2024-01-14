#pragma once
#include "ViewportData.h"
#include "ShaderStructs.h"
#include "PostProcessor.h"
#include "GBufferPass.h"
#include "GPUDrivenGBufferPass.h"
#include "SkyPass.h"
#include "DeferredLightingPass.h"
#include "VolumetricLightingPass.h"
#include "TiledDeferredLightingPass.h"
#include "ClusteredDeferredLightingPass.h"
#include "DDGI.h"
#include "GPUDebugPrinter.h"
#include "HelperPasses.h"
#include "PickingPass.h"
#include "DecalsPass.h"
#include "OceanRenderer.h"
#include "AccelerationStructure.h"
#include "ShadowRenderer.h"
#include "PathTracingPass.h"
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

	class Renderer
	{
		enum class RendererPathType : uint8
		{
			RegularDeferred,
			TiledDeferred,
			ClusteredDeferred,
			PathTracing
		};

	public:

		Renderer(entt::registry& reg, GfxDevice* gfx, uint32 width, uint32 height);
		~Renderer();

		void Tick(Camera const* camera);
		void Update(float dt);
		void Render();
		void SetViewportData(ViewportData const& vp);

		void OnResize(uint32 w, uint32 h);
		void OnRenderResolutionChanged(uint32 w, uint32 h);
		void OnSceneInitialized();
		void OnRightMouseClicked(int32 x, int32 y);
		void OnTakeScreenshot(char const*);

		PickingData const& GetPickingData() const { return picking_data; }
		Vector2u GetDisplayResolution() const { return Vector2u(display_width, display_height); }

	private:
		entt::registry& reg;
		GfxDevice* gfx;
		RGResourcePool resource_pool;

		Camera const* camera;
		Vector2 camera_jitter;

		uint32 const backbuffer_count;
		uint32 backbuffer_index;
		uint32 display_width;
		uint32 display_height;
		uint32 render_width;
		uint32 render_height;

		std::unique_ptr<GfxTexture> final_texture;

		FrameCBuffer frame_cbuf_data{};
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
		ShadowRenderer shadow_renderer;
		PostProcessor postprocessor;
		DDGI		  ddgi;
		PathTracingPass path_tracer;

		//ray tracing
		bool ray_tracing_supported = false;
		AccelerationStructure accel_structure;
		GfxDescriptor tlas_srv;

		//picking
		bool update_picking_data = false;
		PickingData picking_data;

		RendererPathType path_type = RendererPathType::RegularDeferred;
		GPUDebugPrinter gpu_debug_printer;

		//misc
		uint32			         volumetric_lights = 0;
		float					 ambient_color[3] = { 1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f };
		float					 wind_dir[3] = { 1.0f, 0.0f, 1.0f };
		float					 wind_speed = 10.0f;
		Vector3					 sun_direction;
		ViewportData			 viewport_data;

		//screenshot
		bool						take_screenshot = false;
		std::string					screenshot_name = "";
		GfxFence					screenshot_fence;
		uint64						screenshot_fence_value = 1;
		std::unique_ptr<GfxBuffer>  screenshot_buffer;

	private:
		void CreateSizeDependentResources();
		void CreateAS();

		void RendererGUI();
		void UpdateSceneBuffers();
		void UpdateFrameConstants(float dt);
		void CameraFrustumCulling();

		void Render_Deferred(RenderGraph& rg);
		void Render_PathTracing(RenderGraph& rg);

		void CopyToBackbuffer(RenderGraph& rg);
		void TakeScreenshot(RenderGraph& rg);
	};
}