#pragma once
#include <memory>
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
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
	
	struct RayTracingSettings
	{
		float32 dt;
		float32 ao_radius;
	};

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

		struct RayTracingProgram
		{
			Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature = nullptr;
			Microsoft::WRL::ComPtr<ID3D12StateObject> state_object = nullptr;
			std::unique_ptr<ShaderTable> shader_table_raygen = nullptr;
			std::unique_ptr<ShaderTable> shader_table_miss = nullptr;
			std::unique_ptr<ShaderTable> shader_table_hit = nullptr;
		};

	public:
		RayTracer(tecs::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height);
		bool IsSupported() const;

		void OnResize(uint32 width, uint32 height);
		void OnSceneInitialized();

		void Update(RayTracingSettings const&);

		void AddRayTracedShadowsPass(RenderGraph&, size_t light_id);
		void AddRayTracedReflectionsPass(RenderGraph&);
		void AddRayTracedAmbientOcclusionPass(RenderGraph&);

	private:
		uint32 width, height;
		tecs::registry& reg;
		GraphicsDevice* gfx;
		D3D12_RAYTRACING_TIER ray_tracing_tier;

		AccelerationStructure accel_structure;
		ConstantBuffer<RayTracingCBuffer> ray_tracing_cbuffer;

		std::unique_ptr<Buffer> global_vb = nullptr;
		std::unique_ptr<Buffer> global_ib = nullptr;
		std::unique_ptr<Buffer> geo_info_sb = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE first_handle;

		RayTracingProgram ray_traced_shadows;
		RayTracingProgram ray_traced_ambient_occlusion;
		RayTracingProgram ray_traced_reflections;

	private:
		void CreateRootSignatures();
		void CreateStateObjects();
		void CreateShaderTables();
	};
	
}