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
#include "../RenderGraph/RenderGraphResourceName.h"
#include "../Graphics/RayTracingUtil.h" 

namespace adria
{
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

			DirectX::XMFLOAT3 base_color;
			float  metallic_factor;
			float  roughness_factor;
			float  emissive_factor;
			float  alpha_cutoff;
		};

	public:
		RayTracer(entt::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
		bool IsSupported() const;
		bool IsFeatureSupported(ERayTracingFeature feature) const;

		void OnResize(uint32 width, uint32 height);
		void OnSceneInitialized();

		uint32 GetAccelStructureHeapIndex() const;
		void ResetPathTracer() { accumulated_frames = 0; }

		void AddRayTracedShadowsPass(RenderGraph&, uint32, RGResourceName);
		void AddRayTracedReflectionsPass(RenderGraph&);
		void AddRayTracedAmbientOcclusionPass(RenderGraph&);
		void AddPathTracingPass(RenderGraph&);
	private:
		uint32 width, height;
		entt::registry& reg;
		GraphicsDevice* gfx;
		D3D12_RAYTRACING_TIER ray_tracing_tier;

		AccelerationStructure accel_structure;

		std::unique_ptr<Buffer> global_vb = nullptr;
		std::unique_ptr<Buffer> global_ib = nullptr;
		std::unique_ptr<Buffer> geo_buffer = nullptr;
		std::unique_ptr<Texture> accumulation_texture = nullptr;

		Microsoft::WRL::ComPtr<ID3D12StateObject> ray_traced_shadows;
		Microsoft::WRL::ComPtr<ID3D12StateObject> ray_traced_ambient_occlusion;
		Microsoft::WRL::ComPtr<ID3D12StateObject> ray_traced_reflections;
		Microsoft::WRL::ComPtr<ID3D12StateObject> path_tracing;

		BlurPass blur_pass;
		float ao_radius = 2.0f;
		float reflection_roughness_scale = 0.0f;
		int32 accumulated_frames = 1;
		int32 max_bounces = 3;
	private:
		void CreateStateObjects();
		void OnLibraryRecompiled(EShaderId shader);
	};
	
}