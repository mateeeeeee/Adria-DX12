#pragma once
#include <DirectXCollision.h>
#include <memory>
#include <optional>
#include "RendererSettings.h"
#include "SceneViewport.h"
#include "Picker.h"
#include "ConstantBuffers.h"
#include "ParticleRenderer.h"
#include "RayTracer.h"
#include "../tecs/Registry.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/Buffer.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/Texture.h"
#include "../Graphics/RenderPass.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/TextureCube.h"
#include "../Graphics/Texture2DArray.h"
#include "../Graphics/GPUProfiler.h"


namespace adria
{
	class Camera;
	class GraphicsDevice;
	struct Light;

	class Renderer
	{
		enum ENullHeapSlot
		{
			TEXTURE2D_SLOT,
			TEXTURECUBE_SLOT,
			TEXTURE2DARRAY_SLOT,
			RWTEXTURE2D_SLOT,
			NULL_HEAP_SIZE
		};

		enum EIBLHeapSlot
		{
			ENV_TEXTURE_SLOT,
			IRMAP_TEXTURE_SLOT,
			BRDF_LUT_TEXTURE_SLOT
		};

		static constexpr uint32 GBUFFER_SIZE = 3;
		static constexpr uint32 SSAO_NOISE_DIM = 8;
		static constexpr uint32 SSAO_KERNEL_SIZE = 16;
		static constexpr uint32 CLUSTER_SIZE_X = 16;
		static constexpr uint32 CLUSTER_SIZE_Y = 16;
		static constexpr uint32 CLUSTER_SIZE_Z = 16;
		static constexpr uint32 CLUSTER_COUNT = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
		static constexpr uint32 CLUSTER_MAX_LIGHTS = 128;
		static constexpr uint32 RESOLUTION = 512;

	public:
		Renderer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
		
		~Renderer();

		void NewFrame(Camera const* camera);

		void Update(float32 dt);

		void SetSceneViewportData(SceneViewport&&);

		void SetProfilerSettings(ProfilerSettings);

		void Render(RendererSettings const&);

		void ResolveToBackbuffer();

		void ResolveToOffscreenFramebuffer();

		void OnResize(uint32 width, uint32 height);

		void OnRightMouseClicked();

		void OnSceneInitialized();

		Texture const& GetOffscreenTexture() const;
		TextureManager& GetTextureManager();
		std::vector<std::string> GetProfilerResults(bool log = false);
		PickingData GetPickingData() const;

		bool IsRayTracingSupported() const { return ray_tracer.IsSupported(); };
		Texture const& GetRayTracingShadowsTexture_Debug() const
		{
			return ray_tracer.GetRayTracingShadowsTexture();
		}
		Texture const& GetRayTracingAOTexture_Debug() const
		{
			return ray_tracer.GetRayTracingAmbientOcclusionTexture();
		}
		Texture const& GetRayTracingReflectionsTexture_Debug() const
		{
			return ray_tracer.GetRayTracingReflectionsTexture();
		}

	private:
		uint32 width, height;
		tecs::registry& reg;
		GraphicsDevice* gfx;

		uint32 const backbuffer_count;
		uint32 backbuffer_index;
		TextureManager texture_manager;
		Camera const* camera;

		ParticleRenderer particle_renderer;
		RayTracer ray_tracer;

		RendererSettings settings;
		GPUProfiler profiler;
		ProfilerSettings profiler_settings;

		SceneViewport current_scene_viewport;
		Picker picker;
		PickingData picking_data;
		bool update_picking_data = false;

		//textures and heaps
		std::unique_ptr<Texture> hdr_render_target;
		std::unique_ptr<Texture> prev_hdr_render_target;
		std::unique_ptr<Texture> depth_target;
		std::unique_ptr<Texture> ldr_render_target;
		std::unique_ptr<Texture> offscreen_ldr_target;
		std::vector<std::unique_ptr<Texture>> gbuffer;
		std::unique_ptr<Texture> shadow_depth_map;
		TextureCube shadow_depth_cubemap;
		Texture2DArray shadow_depth_cascades;
		std::unique_ptr<Texture> ao_texture;
		std::unique_ptr<Texture> hbao_random_texture;
		std::unique_ptr<Texture> ssao_random_texture;
		std::unique_ptr<Texture> velocity_buffer;
		std::unique_ptr<Texture> blur_intermediate_texture;
		std::unique_ptr<Texture> blur_final_texture;
		std::unique_ptr<Texture> bloom_extract_texture;
		std::unique_ptr<Texture> uav_target;
		std::unique_ptr<Texture> debug_tiled_texture;
		std::array<std::unique_ptr<Texture>, 2> postprocess_textures;
		bool postprocess_index = false;
		std::array<std::unique_ptr<Texture>, 2> ping_pong_phase_textures;
		bool pong_phase = false;
		std::array<std::unique_ptr<Texture>, 2> ping_pong_spectrum_textures;
		bool pong_spectrum = false;
		std::unique_ptr<Texture> ocean_normal_map;
		std::unique_ptr<Texture> ocean_initial_spectrum;

		std::unique_ptr<DescriptorHeap> rtv_heap;
		std::unique_ptr<DescriptorHeap> srv_heap;
		std::unique_ptr<DescriptorHeap> dsv_heap;
		std::unique_ptr<DescriptorHeap> uav_heap;
		std::unique_ptr<DescriptorHeap> null_srv_heap; 
		std::unique_ptr<DescriptorHeap> null_uav_heap;
		uint32 srv_heap_index = 0;
		uint32 uav_heap_index = 0;
		uint32 rtv_heap_index = 0;
		uint32 dsv_heap_index = 0;
		std::unique_ptr<DescriptorHeap> constant_srv_heap;
		std::unique_ptr<DescriptorHeap> constant_dsv_heap;
		std::unique_ptr<DescriptorHeap> constant_uav_heap;

		//Render Passes
		RenderPass gbuffer_render_pass;
		RenderPass decal_pass;
		RenderPass ssao_render_pass;
		RenderPass hbao_render_pass;
		RenderPass ambient_render_pass;
		RenderPass lighting_render_pass;
		RenderPass shadow_map_pass;
		std::array<RenderPass, 6> shadow_cubemap_passes;
		std::vector<RenderPass> shadow_cascades_passes;
		std::array<RenderPass, 2> postprocess_passes;
		RenderPass forward_render_pass;
		RenderPass particle_pass;
		RenderPass velocity_buffer_pass;
		RenderPass fxaa_render_pass;
		RenderPass offscreen_resolve_pass;

		//Persistent cbuffers
		ConstantBuffer<FrameCBuffer> frame_cbuffer;
		FrameCBuffer frame_cbuf_data;
		ConstantBuffer<PostprocessCBuffer> postprocess_cbuffer;
		PostprocessCBuffer postprocess_cbuf_data;
		ConstantBuffer<ComputeCBuffer> compute_cbuffer;
		ComputeCBuffer compute_cbuf_data;
		ConstantBuffer<WeatherCBuffer> weather_cbuffer;
		WeatherCBuffer weather_cbuf_data;
		
		//Persistent sbuffers
		Buffer clusters;
		Buffer light_counter;
		Buffer light_list;
		Buffer light_grid;
		std::unique_ptr<Buffer> bokeh;
		Buffer bokeh_counter;

		std::unique_ptr<Texture> sun_target;
		std::array<DirectX::XMVECTOR, 16> ssao_kernel{};
		DirectX::BoundingBox light_bounding_box;
		DirectX::BoundingFrustum light_bounding_frustum;
		std::optional<DirectX::BoundingSphere> scene_bounding_sphere = std::nullopt;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> lens_flare_textures;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> clouds_textures;

		Microsoft::WRL::ComPtr<ID3D12CommandSignature> bokeh_command_signature;
		std::unique_ptr<Buffer> bokeh_indirect_draw_buffer;
		std::unique_ptr<Buffer> counter_reset_buffer;

		TEXTURE_HANDLE hex_bokeh_handle = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE oct_bokeh_handle = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE circle_bokeh_handle = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE cross_bokeh_handle = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE foam_handle = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE perlin_handle = INVALID_TEXTURE_HANDLE;

		bool recreate_clusters = true;

		std::unique_ptr<DescriptorHeap> ibl_heap;
		Microsoft::WRL::ComPtr<ID3D12Resource> env_texture;
		Microsoft::WRL::ComPtr<ID3D12Resource> irmap_texture;
		Microsoft::WRL::ComPtr<ID3D12Resource> brdf_lut_texture;
	    bool ibl_textures_generated = false;

		std::shared_ptr<Buffer>	cube_vb = nullptr;
		std::shared_ptr<Buffer>	cube_ib = nullptr;
	private:

		void CreateDescriptorHeaps();
		void CreateResolutionDependentResources(uint32 width, uint32 height);
		void CreateOtherResources();
		void CreateRenderPasses(uint32 width, uint32 height);

		void CreateIBLTextures();
		void UploadData();
		
		void UpdateConstantBuffers(float32 dt);
		void UpdateOcean(ID3D12GraphicsCommandList4* cmd_list);
		void UpdateParticles(float32 dt);
		void CameraFrustumCulling();
		void LightFrustumCulling(ELightType type);

		void PassPicking(ID3D12GraphicsCommandList4* cmd_list);
		void PassGBuffer(ID3D12GraphicsCommandList4* cmd_list);
		void PassDecals(ID3D12GraphicsCommandList4* cmd_list);
		void PassSSAO(ID3D12GraphicsCommandList4* cmd_list);
		void PassHBAO(ID3D12GraphicsCommandList4* cmd_list);
		void PassRTAO(ID3D12GraphicsCommandList4* cmd_list);
		void PassAmbient(ID3D12GraphicsCommandList4* cmd_list);
		void PassDeferredLighting(ID3D12GraphicsCommandList4* cmd_list); 
		void PassDeferredTiledLighting(ID3D12GraphicsCommandList4* cmd_list);
		void PassDeferredClusteredLighting(ID3D12GraphicsCommandList4* cmd_list);
		void PassForward(ID3D12GraphicsCommandList4* cmd_list); 
		void PassPostprocess(ID3D12GraphicsCommandList4* cmd_list);

		void PassShadowMapDirectional(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassShadowMapSpot(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassShadowMapPoint(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassShadowMapCascades(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassShadowMapCommon(ID3D12GraphicsCommandList4* cmd_list);
		void PassVolumetric(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		
		void PassForwardCommon(ID3D12GraphicsCommandList4* cmd_list, bool transparent);
		void PassSky(ID3D12GraphicsCommandList4* cmd_list);
		void PassOcean(ID3D12GraphicsCommandList4* cmd_list);
		void PassParticles(ID3D12GraphicsCommandList4* cmd_list);
		
		void PassLensFlare(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassVolumetricClouds(ID3D12GraphicsCommandList4* cmd_list);
		void PassSSR(ID3D12GraphicsCommandList4* cmd_list);
		void PassDepthOfField(ID3D12GraphicsCommandList4* cmd_list);
		void PassGenerateBokeh(ID3D12GraphicsCommandList4* cmd_list);
		void PassDrawBokeh(ID3D12GraphicsCommandList4* cmd_list);
		void PassBloom(ID3D12GraphicsCommandList4* cmd_list);
		void PassGodRays(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassVelocityBuffer(ID3D12GraphicsCommandList4* cmd_list);
		void PassMotionBlur(ID3D12GraphicsCommandList4* cmd_list);
		void PassFXAA(ID3D12GraphicsCommandList4* cmd_list);
		void PassTAA(ID3D12GraphicsCommandList4* cmd_list);
		void PassFog(ID3D12GraphicsCommandList4* cmd_list);
		void PassToneMap(ID3D12GraphicsCommandList4* cmd_list);

		void DrawSun(ID3D12GraphicsCommandList4* cmd_list, tecs::entity sun);
		//result in blur final 
		void BlurTexture(ID3D12GraphicsCommandList4* cmd_list, Texture const& texture);
		//result in current render target
		void CopyTexture(ID3D12GraphicsCommandList4* cmd_list, Texture const& texture, EBlendMode mode = EBlendMode::None);
		
		void AddTextures(ID3D12GraphicsCommandList4* cmd_list, Texture const& texture1, Texture const& texture2, EBlendMode mode = EBlendMode::None);

		void GenerateMips(ID3D12GraphicsCommandList4* cmd_list, Texture const& texture,
			D3D12_RESOURCE_STATES start_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATES end_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	};
}