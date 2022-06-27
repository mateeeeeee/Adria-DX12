#pragma once
#include "RendererSettings.h"
#include "ViewportData.h"
#include "ConstantBuffers.h"
#include "Postprocessor.h"
#include "GBufferPass.h"
#include "AmbientPass.h"
#include "SkyPass.h"
#include "LightingPass.h"
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
#include "OceanRenderer.h"
#include "ParticleRenderer.h"
#include "RayTracer.h"
#include "../Graphics/ShaderUtility.h"
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

	class RenderGraphRenderer
	{
		enum ENullHeapSlot
		{
			NULL_HEAP_SLOT_TEXTURE2D,
			NULL_HEAP_SLOT_TEXTURECUBE,
			NULL_HEAP_SLOT_TEXTURE2DARRAY,
			NULL_HEAP_SLOT_RWTEXTURE2D,
			NULL_HEAP_SIZE
		};
		static constexpr uint32 SSAO_KERNEL_SIZE = SSAOPass::KERNEL_SIZE;

	public:

		RenderGraphRenderer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
		~RenderGraphRenderer();

		void NewFrame(Camera const* camera);
		void Update(float32 dt);
		void Render(RendererSettings const&);

		void SetProfilerSettings(ProfilerSettings const&);
		void SetViewportData(ViewportData&& vp)
		{
			viewport_data = std::move(vp);
		}

		void OnResize(uint32 w, uint32 h);
		void OnSceneInitialized();
		void OnRightMouseClicked(int32 x, int32 y);
		void OnParticleEmitterAdded(size_t emitter_id);
		void OnParticleEmitterRemoved(size_t emitter_id);

		Texture const* GetFinalTexture() const 
		{ 
			return final_texture.get(); 
		}
		TextureManager& GetTextureManager();
		PickingData const& GetPickingData() const { return picking_data; }
		std::vector<TimeStamp> GetProfilerResults() const
		{
			return gpu_profiler.GetProfilerResults(gfx->GetDefaultCommandList());
		}

		bool IsRayTracingSupported() const { return ray_tracer.IsSupported(); }
		Texture const* GetRTAODebugTexture() const 
		{
#ifdef _DEBUG
			return rtao_debug_texture.get();
#else 
			return nullptr;
#endif
		}
	private:
		tecs::registry& reg;
		GraphicsDevice* gfx;
		RGResourcePool resource_pool;

		CPUProfiler cpu_profiler;
		GPUProfiler gpu_profiler;

		RendererSettings render_settings;
		ProfilerSettings profiler_settings = NO_PROFILING;
		ViewportData viewport_data;

		TextureManager texture_manager;
		Camera const* camera;

		uint32 const backbuffer_count;
		uint32 backbuffer_index;
		uint32 width;
		uint32 height;

		//resources
		std::unique_ptr<Texture> final_texture;

		//Persistent constant buffers
		ConstantBuffer<FrameCBuffer> frame_cbuffer;
		ConstantBuffer<PostprocessCBuffer> postprocess_cbuffer;
		ConstantBuffer<ComputeCBuffer> compute_cbuffer;
		ConstantBuffer<WeatherCBuffer> weather_cbuffer;
		//misc
		std::unique_ptr<DescriptorHeap> null_heap;
		std::array<DirectX::XMVECTOR, SSAO_KERNEL_SIZE> ssao_kernel{};
		bool update_picking_data = false;
		PickingData picking_data;

		std::unique_ptr<Texture> env_texture;
		std::unique_ptr<Texture> irmap_texture;
		std::unique_ptr<Texture> brdf_lut_texture;
		bool ibl_generated = false;
#ifdef _DEBUG
		std::unique_ptr<Texture> rtao_debug_texture;
#endif

		//passes
		GBufferPass  gbuffer_pass;
		SSAOPass	 ssao_pass;
		HBAOPass     hbao_pass;
		AmbientPass  ambient_pass;
		ToneMapPass  tonemap_pass;
		FXAAPass	 fxaa_pass;
		SkyPass		 sky_pass;
		LightingPass lighting_pass;
		TiledLightingPass tiled_lighting_pass;
		ClusteredLightingPass clustered_lighting_pass;
		CopyToTexturePass copy_to_texture_pass;
		AddTexturesPass add_textures_pass;
		ShadowPass    shadow_pass;
		PickingPass picking_pass;
		DecalsPass decals_pass;
		OceanRenderer  ocean_renderer;
		ParticleRenderer particle_renderer;
		RayTracer ray_tracer;
		Postprocessor postprocessor;
	private:
		void CreateNullHeap();
		void CreateSizeDependentResources();
		void UpdatePersistentConstantBuffers(float32 dt);
		void CameraFrustumCulling();
		void GenerateIBLTextures();

		void ResolveToBackbuffer(RenderGraph& rg);
		void ResolveToTexture(RenderGraph& rg);
	};
}