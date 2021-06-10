#pragma once
#include <DirectXCollision.h>
#include <memory>
#include <optional>
#include "RendererSettings.h"
#include "ConstantBuffers.h"
#include "../tecs/Registry.h"
#include "../Graphics/TextureManager.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/RenderPass.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/TextureCube.h"
#include "../Graphics/Texture2DArray.h"
#include "../Graphics/StructuredBuffer.h"



namespace adria
{
	class Camera;
	class GraphicsCoreDX12;
	template<typename T>
	class StructuredBuffer;
	struct Light;


	class Renderer
	{
		enum NULL_HEAP_SLOT
		{
			TEXTURE2D,
			TEXTURECUBE,
			TEXTURE2DARRAY,
			RWTEXTURE2D,
			NULL_HEAP_SIZE
		};

		static constexpr u32 GBUFFER_SIZE = 3;
		static constexpr u32 SSAO_NOISE_DIM = 8;
		static constexpr u32 SSAO_KERNEL_SIZE = 16;
		static constexpr u32 CLUSTER_SIZE_X = 16;
		static constexpr u32 CLUSTER_SIZE_Y = 16;
		static constexpr u32 CLUSTER_SIZE_Z = 16;
		static constexpr u32 CLUSTER_COUNT = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
		static constexpr u32 CLUSTER_MAX_LIGHTS = 128;

	public:
		Renderer(tecs::registry& reg, GraphicsCoreDX12* gfx, u32 width, u32 height);
		
		~Renderer();

		void NewFrame(Camera const* camera);

		void Update(f32 dt);

		void Render(RendererSettings const&);

		void ResolveToBackbuffer();

		void ResolveToOffscreenFramebuffer();

		void OnResize(u32 width, u32 height);

		void LoadTextures();

		TextureManager& GetTextureManager();

		Texture2D GetOffscreenTexture() const;

	private:
		u32 width, height;
		tecs::registry& reg;
		GraphicsCoreDX12* gfx;
		u32 const backbuffer_count;
		u32 backbuffer_index;
		TextureManager texture_manager;
		Camera const* camera;

		RendererSettings settings;

		std::unordered_map<Shader, ShaderBlob> shader_map;
		std::unordered_map<RootSig, Microsoft::WRL::ComPtr<ID3D12RootSignature>> rs_map;
		std::unordered_map<PSO, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pso_map;

		//textures and heaps
		Texture2D hdr_render_target;
		Texture2D prev_hdr_render_target;
		Texture2D depth_stencil_target;
		Texture2D ldr_render_target;
		Texture2D offscreen_ldr_target;
		std::vector<Texture2D> gbuffer;
		Texture2D shadow_depth_map;
		TextureCube shadow_depth_cubemap;
		Texture2DArray shadow_depth_cascades;
		Texture2D ssao_texture;
		Texture2D noise_texture;
		Texture2D blur_intermediate_texture;
		Texture2D blur_final_texture;
		Texture2D bloom_extract_texture;
		Texture2D uav_target;
		Texture2D debug_tiled_texture;
		std::array<Texture2D, 2> postprocess_textures;
		bool postprocess_index = false;
		std::unique_ptr<DescriptorHeap> rtv_heap;
		std::unique_ptr<DescriptorHeap> srv_heap;
		std::unique_ptr<DescriptorHeap> dsv_heap;
		std::unique_ptr<DescriptorHeap> uav_heap;
		std::unique_ptr<DescriptorHeap> null_srv_heap; 
		std::unique_ptr<DescriptorHeap> null_uav_heap; 
		u32 srv_heap_index = 0;
		u32 uav_heap_index = 0;
		u32 rtv_heap_index = 0;
		u32 dsv_heap_index = 0;

		//Render Passes
		RenderPass gbuffer_render_pass;
		RenderPass ssao_render_pass;
		RenderPass ambient_render_pass;
		RenderPass lighting_render_pass;
		RenderPass shadow_map_pass;
		std::array<RenderPass, 6> shadow_cubemap_passes;
		std::vector<RenderPass> shadow_cascades_passes;
		std::array<RenderPass, 2> postprocess_passes;
		RenderPass forward_render_pass;
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
		//Transient cbuffers
		ObjectCBuffer object_cbuf_data{};		
		MaterialCBuffer material_cbuf_data{};
		LightCBuffer light_cbuf_data{};
		ShadowCBuffer shadow_cbuf_data{};
		DynamicAllocation object_allocation;
		DynamicAllocation material_allocation;
		DynamicAllocation light_allocation;
		DynamicAllocation shadow_allocation;
		
		//Persistent sbuffers
		StructuredBuffer<ClusterAABB>	clusters;
		StructuredBuffer<u32>			light_counter;
		StructuredBuffer<u32>			light_list;
		StructuredBuffer<LightGrid>  	light_grid;
		
		std::array<DirectX::XMVECTOR, 16> ssao_kernel{};
		DirectX::BoundingBox light_bounding_box;
		DirectX::BoundingFrustum light_bounding_frustum;
		std::optional<DirectX::BoundingSphere> scene_bounding_sphere = std::nullopt;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> lens_flare_textures;
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> clouds_textures;
		bool recreate_clusters = true;

	private:

		void LoadShaders();
		void CreatePipelineStateObjects();
		void CreateDescriptorHeaps();
		void CreateViews(u32 width, u32 height);
		void CreateRenderPasses(u32 width, u32 height);
		
		void UpdateConstantBuffers(f32 dt);
		void CameraFrustumCulling();
		void LightFrustumCulling(LightType type);

		void PassGBuffer(ID3D12GraphicsCommandList4* cmd_list);
		void PassSSAO(ID3D12GraphicsCommandList4* cmd_list);
		void PassAmbient(ID3D12GraphicsCommandList4* cmd_list);
		void PassDeferredLighting(ID3D12GraphicsCommandList4* cmd_list); 
		void PassDeferredTiledLighting(ID3D12GraphicsCommandList4* cmd_list);
		void PassDeferredClusteredLighting(ID3D12GraphicsCommandList4* cmd_list);
		void PassForward(ID3D12GraphicsCommandList4* cmd_list); 
		void PassPostprocess(ID3D12GraphicsCommandList4* cmd_list);
		//postprocess

		void PassShadowMapDirectional(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassShadowMapSpot(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassShadowMapPoint(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassShadowMapCascades(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassVolumetric(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		
		void PassForwardCommon(ID3D12GraphicsCommandList4* cmd_list, bool transparent);
		void PassSkybox(ID3D12GraphicsCommandList4* cmd_list);

		void PassLensFlare(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassVolumetricClouds(ID3D12GraphicsCommandList4* cmd_list);
		void PassSSR(ID3D12GraphicsCommandList4* cmd_list);
		void PassDepthOfField(ID3D12GraphicsCommandList4* cmd_list);
		void PassBloom(ID3D12GraphicsCommandList4* cmd_list);
		void PassGodRays(ID3D12GraphicsCommandList4* cmd_list, Light const& light);
		void PassMotionBlur(ID3D12GraphicsCommandList4* cmd_list);
		void PassFXAA(ID3D12GraphicsCommandList4* cmd_list);
		void PassTAA(ID3D12GraphicsCommandList4* cmd_list);
		void PassFog(ID3D12GraphicsCommandList4* cmd_list);
		void PassToneMap(ID3D12GraphicsCommandList4* cmd_list);

		//result in blur final 
		void BlurTexture(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture);
		//result in current render target
		void CopyTexture(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture, BlendMode mode = BlendMode::eNone);
		
		void AddTextures(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& texture1, Texture2D const& texture2, BlendMode mode = BlendMode::eNone);
	};
}