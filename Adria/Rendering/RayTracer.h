#pragma once
#include <memory>
#include "../Graphics/RayTracingUtil.h"
#include "ConstantBuffers.h"
#include "../tecs/Registry.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/ConstantBuffer.h"


namespace adria
{
	class RayTracer
	{

	public:

        RayTracer(tecs::registry& reg, GraphicsCoreDX12* gfx, uint32 width, uint32 height);

        bool IsSupported() const;

        void BuildAccelerationStructures();

		Texture2D& RayTraceShadows(ID3D12GraphicsCommandList4* cmd_list, Texture2D const& depth_srv,
			D3D12_GPU_VIRTUAL_ADDRESS frame_cbuf_address,
			D3D12_GPU_VIRTUAL_ADDRESS light_cbuf_address);

		Texture2D const& GetRayTracingShadowsTexture_Debug() const
		{
			return rt_shadows_output;
		}

    private:
        uint32 width, height;
        tecs::registry& reg;
        GraphicsCoreDX12* gfx;
		bool ray_tracing_supported;

		std::unique_ptr<DescriptorHeap> dxr_heap = nullptr;
		//Persistent cbuffer
		ConstantBuffer<RayTracingCBuffer> ray_tracing_cbuffer;
		RayTracingCBuffer ray_tracing_cbuf_data;

        Microsoft::WRL::ComPtr<ID3D12Resource> blas = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> tlas = nullptr;
        uint64 tlas_size = 0;

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
		
	private:

		void CreateResources();
		void CreateRootSignatures();
		void CreateStateObjects();
		void CreateShaderTables();

        void BuildBottomLevelAS();
        void BuildTopLevelAS();
	};
}