#pragma once
#include <memory>
#include "../Graphics/RayTracingUtil.h"
#include "ConstantBuffers.h"
#include "../tecs/Registry.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/StructuredBuffer.h"


namespace adria
{
	struct RayTracingParams
	{
		float32 dt;
		float32 ao_radius;
	};

	//used by Renderer class for simple ray tracing features: shadows, ambient occlusion
	class RayTracer
	{
	public:

        RayTracer(tecs::registry& reg, GraphicsCoreDX12* gfx, uint32 width, uint32 height);
        bool IsSupported() const;
        void BuildAccelerationStructures();
		void Update(RayTracingParams const&);

		void RayTraceShadows(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& depth_srv,
			D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address,
			D3D12_GPU_VIRTUAL_ADDRESS light_cbuf_address, bool soft_shadows);
		void RayTraceAmbientOcclusion(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& depth, Texture2D const& normal_gbuf, D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address);

		Texture2D const& GetRayTracingShadowsTexture() const
		{
			return rt_shadows_output;
		}
		Texture2D const& GetRayTracingAmbientOcclusionTexture() const
		{
			return rtao_output;
		}

    private:
        uint32 width, height;
        tecs::registry& reg;
        GraphicsCoreDX12* gfx;
		bool ray_tracing_supported;

		std::unique_ptr<DescriptorHeap> dxr_heap = nullptr;
		ConstantBuffer<RayTracingCBuffer> ray_tracing_cbuffer;
		RayTracingCBuffer ray_tracing_cbuf_data;

        Microsoft::WRL::ComPtr<ID3D12Resource> blas = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> tlas = nullptr;
        uint64 tlas_size = 0;

		std::unique_ptr<VertexBuffer> global_vb = nullptr;
		std::unique_ptr<IndexBuffer>  global_ib = nullptr;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rt_shadows_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rt_shadows_state_object = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_raygen = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_miss = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_hit = nullptr;
		Texture2D rt_shadows_output;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rtao_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rtao_state_object = nullptr;
		std::unique_ptr<ShaderTable> rtao_shader_table_raygen = nullptr;
		std::unique_ptr<ShaderTable> rtao_shader_table_miss = nullptr;
		std::unique_ptr<ShaderTable> rtao_shader_table_hit = nullptr;
		Texture2D rtao_output;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rts_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rts_state_object = nullptr;
		std::unique_ptr<ShaderTable> rts_shader_table_raygen = nullptr;
		std::unique_ptr<ShaderTable> rts_shader_table_miss = nullptr;
		std::unique_ptr<ShaderTable> rts_shader_table_hit = nullptr;
		Texture2D rts_output;
		

	private:

        void BuildBottomLevelAS();
        void BuildTopLevelAS();

		void CreateGlobalBuffers();
		void CreateResources();
		void CreateRootSignatures();
		void CreateStateObjects();
		void CreateShaderTables();

	};
}