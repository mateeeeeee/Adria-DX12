#pragma once
#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include "BlurPass.h"
#include "ConstantBuffers.h"
#include "AccelerationStructure.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"
#include "../Graphics/ConstantBuffer.h"
#include "../Graphics/RayTracingUtil.h" 

namespace adria
{
	
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class TextureManager;
	class GraphicsDevice;
	class Texture;
	class Buffer;
	struct Light;

	enum class ERayTracingFeature : uint8 
	{
		Shadows,
		Reflections,
		AmbientOcclusion,
		PathTracing
	};

	class RayTracer
	{
		struct GeoInfo
		{
			uint32	vertex_offset;
			uint32	index_offset;
			int32	albedo_idx;
			int32	normal_idx;
			int32	metallic_roughness_idx;
			int32	emissive_idx;
		};

	public:
		RayTracer(entt::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
		bool IsSupported() const;
		bool IsFeatureSupported(ERayTracingFeature feature) const;

		void OnResize(uint32 width, uint32 height);
		void OnSceneInitialized();

		void ResetPathTracer() { accumulated_frames = 0; }
		void Update(float32 dt);

		void AddRayTracedShadowsPass(RenderGraph&, Light const&, size_t);
		void AddRayTracedReflectionsPass(RenderGraph&, D3D12_CPU_DESCRIPTOR_HANDLE);
		void AddRayTracedAmbientOcclusionPass(RenderGraph&);
		void AddPathTracingPass(RenderGraph&, D3D12_CPU_DESCRIPTOR_HANDLE);
#ifdef _DEBUG
		Texture const* GetRTAODebugTexture() const
		{
			return rtao_debug_texture.get();
		}
		Texture const* GetRTSDebugTexture() const
		{
			return rts_debug_texture.get();
		}
		Texture const* GetRTRDebugTexture() const
		{
			return rtr_debug_texture.get();
		}
#endif
	private:
		uint32 width, height;
		entt::registry& reg;
		GraphicsDevice* gfx;
		D3D12_RAYTRACING_TIER ray_tracing_tier;

		AccelerationStructure accel_structure;
		ConstantBuffer<RayTracingCBuffer> ray_tracing_cbuffer;

		std::unique_ptr<Buffer> global_vb = nullptr;
		std::unique_ptr<Buffer> global_ib = nullptr;
		std::unique_ptr<Buffer> geo_buffer = nullptr;

		Microsoft::WRL::ComPtr<ID3D12StateObject> ray_traced_shadows;
		Microsoft::WRL::ComPtr<ID3D12StateObject> ray_traced_ambient_occlusion;
		Microsoft::WRL::ComPtr<ID3D12StateObject> ray_traced_reflections;

		Microsoft::WRL::ComPtr<ID3D12StateObject> path_tracing;
		std::unique_ptr<Texture> accumulation_texture;
		int32 accumulated_frames = 1;

		BlurPass blur_pass;
#ifdef _DEBUG
		std::unique_ptr<Texture> rtao_debug_texture;
		std::unique_ptr<Texture> rts_debug_texture;
		std::unique_ptr<Texture> rtr_debug_texture;
#endif
		
	private:
		void CreateStateObjects();
		void OnLibraryRecompiled(EShader shader);

#ifdef _DEBUG
		void AddRayTracedAmbientOcclusionDebugPass(RenderGraph&);
		void AddRayTracedShadowsDebugPass(RenderGraph&, size_t);
		void AddRayTracedReflectionsDebugPass(RenderGraph&);
#endif
	};
	
}