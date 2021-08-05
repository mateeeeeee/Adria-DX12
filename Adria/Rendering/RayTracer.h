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
        RayTracer(tecs::registry& reg, GraphicsCoreDX12* gfx, u32 width, u32 height) : reg{ reg }, gfx{ gfx }, width{ width }, height{ height }
        {
            ID3D12Device* device = gfx->Device();

            D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
            HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
            if (FAILED(hr) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
            {
                Log::Info("Ray Tracing is not supported! All Ray Tracing calls will be silently ignored!\n");
                ray_tracing_supported = false;
            }
        }

        bool IsSupported() const;

        void BuildAccelerationStructures();


    private:
        u32 width, height;
        tecs::registry& reg;
        GraphicsCoreDX12* gfx;

        Microsoft::WRL::ComPtr<ID3D12Resource> blas = nullptr;
        Microsoft::WRL::ComPtr<ID3D12Resource> tlas = nullptr;
        u64 tlas_size = 0;
        bool ray_tracing_supported = true;

	private:

        void BuildBottomLevelAS();

        void BuildTopLevelAS();

	};
}