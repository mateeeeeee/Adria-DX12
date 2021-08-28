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
		struct AccelerationStructureBuffers
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> scratch_buffer;
			Microsoft::WRL::ComPtr<ID3D12Resource> result_buffer;
			Microsoft::WRL::ComPtr<ID3D12Resource> instance_desc_buffer;    // Used only for top-level AS
		};
	public:

        RayTracer(tecs::registry& reg, GraphicsCoreDX12* gfx, u32 width, u32 height);

        bool IsSupported() const;

        void BuildAccelerationStructures();


    private:
        u32 width, height;
        tecs::registry& reg;
        GraphicsCoreDX12* gfx;
		bool ray_tracing_supported;

        Microsoft::WRL::ComPtr<ID3D12Resource> blas = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> tlas = nullptr;
        u64 tlas_size = 0;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rt_shadows_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rt_shadows_state_object = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_raygen = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_miss = nullptr;
		std::unique_ptr<ShaderTable> rt_shadows_shader_table_hit = nullptr;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rtao_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rtao_state_object = nullptr;

		
	private:

        void BuildBottomLevelAS();
        void BuildTopLevelAS();

		void CreateRootSignatures();
		void CreateStateObjects();
		void CreateShaderTables();
	};
}