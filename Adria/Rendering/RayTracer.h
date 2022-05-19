#pragma once
#include <memory>
#include "ConstantBuffers.h"
#include "../Graphics/RayTracingUtil.h"
#include "../Graphics/Texture.h"
#include "../Graphics/ConstantBuffer.h"


namespace adria
{
	struct RayTracingSettings
	{
		float32 dt;
		float32 ao_radius;
	};

	class Buffer;
	
	namespace tecs
	{
		class registry;
	}
	

	class RayTracer
	{
		struct GeoInfo
		{
			uint32 vertex_offset;
			uint32 index_offset;

			int32 albedo_idx;
			int32 normal_idx;
			int32 metallic_roughness_idx;
			int32 emissive_idx;
		};

	public:

        RayTracer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
        bool IsSupported() const;
        void OnSceneInitialized();
		void Update(RayTracingSettings const&);

		void RayTraceShadows(ID3D12GraphicsCommandList4* cmd_list, Texture const& depth_srv,
			D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address, D3D12_GPU_VIRTUAL_ADDRESS light_cbuf_address, bool soft_shadows);
		void RayTraceAmbientOcclusion(ID3D12GraphicsCommandList4* cmd_list, Texture const& depth, Texture const& normal_gbuf, D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address);
		void RayTraceReflections(ID3D12GraphicsCommandList4* cmd_list, Texture const& depth, D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address, D3D12_CPU_DESCRIPTOR_HANDLE envmap_handle);

		Texture const& GetRayTracingShadowsTexture() const
		{
			return *rt_shadows_output;
		}
		Texture const& GetRayTracingAmbientOcclusionTexture() const
		{
			return *rtao_output;
		}
		Texture const& GetRayTracingReflectionsTexture() const
		{
			return *rtr_output;
		}

    private:
        uint32 width, height;
        tecs::registry& reg;
        GraphicsDevice* gfx;
		D3D12_RAYTRACING_TIER ray_tracing_tier;
		D3D12_CPU_DESCRIPTOR_HANDLE envmap_handle;

		std::unique_ptr<DescriptorHeap> dxr_heap = nullptr;
		ConstantBuffer<RayTracingCBuffer> ray_tracing_cbuffer;
		RayTracingCBuffer ray_tracing_cbuf_data;

        Microsoft::WRL::ComPtr<ID3D12Resource> blas = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> tlas = nullptr;
        uint64 tlas_size = 0;

		std::unique_ptr<Buffer> global_vb = nullptr;
		std::unique_ptr<Buffer> global_ib = nullptr;
		std::unique_ptr<Buffer> geo_info_sb = nullptr;

		//group this kind of quint inside a struct
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rt_shadows_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rt_shadows_state_object = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_raygen = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_miss = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_hit = nullptr;
		std::unique_ptr<Texture> rt_shadows_output;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rtao_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rtao_state_object = nullptr;
		std::unique_ptr<ShaderTable> rtao_shader_table_raygen = nullptr;
		std::unique_ptr<ShaderTable> rtao_shader_table_miss = nullptr;
		std::unique_ptr<ShaderTable> rtao_shader_table_hit = nullptr;
		std::unique_ptr<Texture> rtao_output;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rtr_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rtr_state_object = nullptr;
		std::unique_ptr<ShaderTable> rtr_shader_table_raygen = nullptr;
		std::unique_ptr<ShaderTable> rtr_shader_table_miss = nullptr;
		std::unique_ptr<ShaderTable> rtr_shader_table_hit = nullptr;
		std::unique_ptr<Texture> rtr_output;

	private:

        void BuildBottomLevelAS();
        void BuildTopLevelAS();

		void CreateResources();
		void CreateRootSignatures();
		void CreateStateObjects();
		void CreateShaderTables();

	};
}