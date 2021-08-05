#pragma once
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

#include "../Logging/Logger.h"
#include "Components.h"

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

        Microsoft::WRL::ComPtr<ID3D12Resource> blas = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> tlas = nullptr;
        u64 tlas_size = 0;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rt_root_signature = nullptr;
		Microsoft::WRL::ComPtr<ID3D12StateObject> rt_state_object = nullptr;

        bool ray_tracing_supported;

	private:

        void BuildBottomLevelAS();
        void BuildTopLevelAS();

		void CreateRootSignatures();
		void CreateStateObjects();

	};
}