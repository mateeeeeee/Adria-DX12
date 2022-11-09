#include "RayTracer.h"
#include "Components.h"
#include "BlackboardData.h"
#include "ShaderCache.h"
#include "RootSignatureCache.h"
#include "entt/entity/registry.hpp"
#include "../RenderGraph/RenderGraph.h"
#include "../Editor/GUICommand.h"
#include "../Graphics/Shader.h"
#include "../Logging/Logger.h"

using namespace DirectX;

namespace adria
{

	RayTracer::RayTracer(entt::registry& reg, GraphicsDevice* gfx, uint32 width, uint32 height)
		: reg(reg), gfx(gfx), width(width), height(height), accel_structure(gfx), 
		blur_pass(width, height)
	{
		ID3D12Device* device = gfx->GetDevice();
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
		HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
		ray_tracing_tier = features5.RaytracingTier;
		if (FAILED(hr) || ray_tracing_tier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		{
			ADRIA_LOG(INFO, "Ray Tracing is not supported! All Ray Tracing calls will be silently ignored!");
			return;
		}
		else if (ray_tracing_tier < D3D12_RAYTRACING_TIER_1_1)
		{
			ADRIA_LOG(INFO, "Ray Tracing Tier is less than Tier 1.1!"
				"Ray Traced Reflections will be silently ignored!");
		}
		OnResize(width, height);
		CreateStateObjects();
		ShaderCache::GetLibraryRecompiledEvent().AddMember(&RayTracer::OnLibraryRecompiled, *this);
	}

	bool RayTracer::IsSupported() const
	{
		return ray_tracing_tier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	}
	bool RayTracer::IsFeatureSupported(ERayTracingFeature feature) const
	{
		switch (feature)
		{
		case ERayTracingFeature::Shadows:
		case ERayTracingFeature::AmbientOcclusion:
			return ray_tracing_tier >= D3D12_RAYTRACING_TIER_1_0;
		case ERayTracingFeature::Reflections:
		case ERayTracingFeature::PathTracing:
			return ray_tracing_tier >= D3D12_RAYTRACING_TIER_1_1;
		default:
			return false;
		}
	}
	void RayTracer::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(w, h);

		TextureDesc accum_desc{};
		accum_desc.width = width;
		accum_desc.height = height;
		accum_desc.format = EFormat::R16G16B16A16_FLOAT;
		accum_desc.bind_flags = EBindFlag::ShaderResource | EBindFlag::UnorderedAccess;
		accum_desc.initial_state = EResourceState::UnorderedAccess;
		accumulation_texture = std::make_unique<Texture>(gfx, accum_desc);
	}
	void RayTracer::OnSceneInitialized()
	{
		if (!IsSupported()) return;

		auto ray_tracing_view = reg.view<Mesh, Transform, Material, RayTracing>();
		std::vector<GeoInfo> geo_info{};
		for (auto entity : ray_tracing_view)
		{
			auto const& [mesh, transform, material, ray_tracing] = ray_tracing_view.get<Mesh, Transform, Material, RayTracing>(entity);
			geo_info.push_back(GeoInfo{
				.vertex_offset = ray_tracing.vertex_offset,
				.index_offset = ray_tracing.index_offset,
				.albedo_idx = (int32)material.albedo_texture,
				.normal_idx = (int32)material.normal_texture,
				.metallic_roughness_idx = (int32)material.metallic_roughness_texture,
				.emissive_idx = (int32)material.emissive_texture,
				.base_color = XMFLOAT3(material.base_color),
				.metallic_factor = material.metallic_factor,
				.roughness_factor = material.roughness_factor,
				.emissive_factor = material.emissive_factor,
				.alpha_cutoff = material.alpha_cutoff
				});
			accel_structure.AddInstance(mesh, transform);
		}
		accel_structure.Build();

		BufferDesc desc = StructuredBufferDesc<GeoInfo>(geo_info.size(), false);
		geo_buffer = std::make_unique<Buffer>(gfx, desc, geo_info.data());

		ID3D12Device5* device = gfx->GetDevice();
		if (RayTracing::rt_vertices.empty() || RayTracing::rt_indices.empty())
		{
			ADRIA_LOG(WARNING, "Ray tracing buffers are empty. This is expected if the meshes are loaded with ray-tracing support off");
			return;
		}

		BufferDesc vb_desc{};
		vb_desc.bind_flags = EBindFlag::ShaderResource;
		vb_desc.misc_flags = EBufferMiscFlag::VertexBuffer | EBufferMiscFlag::BufferStructured;
		vb_desc.size = RayTracing::rt_vertices.size() * sizeof(CompleteVertex);
		vb_desc.stride = sizeof(CompleteVertex);

		BufferDesc ib_desc{};
		ib_desc.bind_flags = EBindFlag::ShaderResource;
		ib_desc.misc_flags = EBufferMiscFlag::IndexBuffer | EBufferMiscFlag::BufferStructured;
		ib_desc.size = RayTracing::rt_indices.size() * sizeof(uint32);
		ib_desc.stride = sizeof(uint32);
		ib_desc.format = EFormat::R32_UINT;

		global_vb = std::make_unique<Buffer>(gfx, vb_desc, RayTracing::rt_vertices.data());
		global_ib = std::make_unique<Buffer>(gfx, ib_desc, RayTracing::rt_indices.data());
	}

	uint32 RayTracer::GetAccelStructureHeapIndex() const
	{
		if (!IsSupported()) return uint32(-1);

		auto device = gfx->GetDevice();
		auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

		uint32 i = (uint32)descriptor_allocator->Allocate();
		device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i), accel_structure.GetTLAS()->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		return i;
	}

	void RayTracer::AddRayTracedShadowsPass(RenderGraph& rg, uint32 light_index, RGResourceName mask_name)
	{
		if (!IsFeatureSupported(ERayTracingFeature::Shadows)) return;

		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct RayTracedShadowsPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadWriteId mask;
		};

		rg.AddPass<RayTracedShadowsPassData>("Ray Traced Shadows Pass",
			[=](RayTracedShadowsPassData& data, RGBuilder& builder)
			{
				data.mask = builder.WriteTexture(mask_name);
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
			},
			[=](RayTracedShadowsPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();
				auto dynamic_allocator = gfx->GetDynamicAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(2);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.mask), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct RayTracedShadowsConstants
				{
					uint32  depth_idx;
					uint32  output_idx;
					uint32  light_idx;
				} constants =
				{
					.depth_idx = i + 0, .output_idx = i + 1,
					.light_idx = light_index 
				};
				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetPipelineState1(ray_traced_shadows.Get());

				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 3, &constants, 0);

				D3D12_DISPATCH_RAYS_DESC dispatch_desc{};
				dispatch_desc.Width = width;
				dispatch_desc.Height = height;
				dispatch_desc.Depth = 1;

				RayTracingShaderTable table(ray_traced_shadows.Get());
				table.SetRayGenShader("RTS_RayGen_Hard");
				table.AddMissShader("RTS_Miss", 0);
				table.AddHitGroup("ShadowAnyHitGroup", 0);
				table.Commit(*gfx->GetDynamicAllocator(), dispatch_desc);
				cmd_list->DispatchRays(&dispatch_desc);

			}, ERGPassType::Compute, ERGPassFlags::ForceNoCull);
	}
	void RayTracer::AddRayTracedReflectionsPass(RenderGraph& rg)
	{
		if (!IsFeatureSupported(ERayTracingFeature::Reflections)) return;

		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct RayTracedReflectionsPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadWriteId output;

			RGBufferReadOnlyId vb;
			RGBufferReadOnlyId ib;
			RGBufferReadOnlyId geo;
		};

		rg.ImportBuffer(RG_RES_NAME(BigVertexBuffer), global_vb.get());
		rg.ImportBuffer(RG_RES_NAME(BigIndexBuffer), global_ib.get());
		rg.ImportBuffer(RG_RES_NAME(BigGeometryBuffer), geo_buffer.get());

		rg.AddPass<RayTracedReflectionsPassData>("Ray Traced Reflections Pass",
			[=](RayTracedReflectionsPassData& data, RGBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = EFormat::R8G8B8A8_UNORM;
				builder.DeclareTexture(RG_RES_NAME(RTR_OutputNoisy), desc);

				data.output = builder.WriteTexture(RG_RES_NAME(RTR_OutputNoisy));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil));
				data.normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal));

				data.vb = builder.ReadBuffer(RG_RES_NAME(BigVertexBuffer));
				data.ib = builder.ReadBuffer(RG_RES_NAME(BigIndexBuffer));
				data.geo = builder.ReadBuffer(RG_RES_NAME(BigGeometryBuffer));
			},
			[=](RayTracedReflectionsPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(5); 
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyBuffer(data.vb), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadOnlyBuffer(data.ib), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 4), ctx.GetReadOnlyBuffer(data.geo), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct RayTracedReflectionsConstants
				{
					float   roughness_scale;
					uint32  depth_idx;
					uint32  output_idx;
					uint32  vertices_idx;
					uint32  indices_idx;
					uint32  geo_infos_idx;
				} constants = 
				{   
					.roughness_scale = roughness_scale,
					.depth_idx = i + 0, .output_idx = i + 1,
					.vertices_idx = i + 2, .indices_idx = i + 3, .geo_infos_idx = i + 4
				};

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 6, &constants, 0);
				cmd_list->SetPipelineState1(ray_traced_reflections.Get());

				D3D12_DISPATCH_RAYS_DESC dispatch_desc{};
				dispatch_desc.Width = width;
				dispatch_desc.Height = height;
				dispatch_desc.Depth = 1;

				RayTracingShaderTable table(ray_traced_reflections.Get());
				table.SetRayGenShader("RTR_RayGen");
				table.AddMissShader("RTR_Miss", 0);
				table.AddHitGroup("RTRClosestHitGroupPrimaryRay", 0);
				table.AddHitGroup("RTRClosestHitGroupReflectionRay", 1);
				table.Commit(*gfx->GetDynamicAllocator(), dispatch_desc);
				cmd_list->DispatchRays(&dispatch_desc);
			}, ERGPassType::Compute, ERGPassFlags::None);

		blur_pass.AddPass(rg, RG_RES_NAME(RTR_OutputNoisy), RG_RES_NAME(RTR_Output), "RTR Denoise");

		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Ray Traced Reflection", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderFloat("Roughness scale", &roughness_scale, 0.0f, 0.25f);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);
	}
	void RayTracer::AddRayTracedAmbientOcclusionPass(RenderGraph& rg)
	{
		if (!IsFeatureSupported(ERayTracingFeature::AmbientOcclusion)) return;

		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct RayTracedAmbientOcclusionPassData
		{
			RGTextureReadOnlyId depth;
			RGTextureReadOnlyId normal;
			RGTextureReadWriteId output;
		};

		rg.AddPass<RayTracedAmbientOcclusionPassData>("Ray Traced Ambient Occlusion Pass",
			[=](RayTracedAmbientOcclusionPassData& data, RGBuilder& builder)
			{
				RGTextureDesc desc{};
				desc.width = width;
				desc.height = height;
				desc.format = EFormat::R8_UNORM;
				builder.DeclareTexture(RG_RES_NAME(RTAO_Output), desc);

				data.output = builder.WriteTexture(RG_RES_NAME(RTAO_Output));
				data.depth = builder.ReadTexture(RG_RES_NAME(DepthStencil), ReadAccess_NonPixelShader);
				data.normal = builder.ReadTexture(RG_RES_NAME(GBufferNormal), ReadAccess_NonPixelShader);
			},
			[=](RayTracedAmbientOcclusionPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(3);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadOnlyTexture(data.depth), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadOnlyTexture(data.normal), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct RayTracedAmbientOcclusionConstants
				{
					uint32  depth_idx;
					uint32  gbuf_normals_idx;
					uint32  output_idx;
					float   ao_radius;
				} constants = 
				{
					.depth_idx = i + 0, .gbuf_normals_idx = i + 1, .output_idx = i + 2,
					.ao_radius = ao_radius
				};
				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetPipelineState1(ray_traced_ambient_occlusion.Get());

				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 4, &constants, 0);

				D3D12_DISPATCH_RAYS_DESC dispatch_desc{};
				dispatch_desc.Width = width;
				dispatch_desc.Height = height;
				dispatch_desc.Depth = 1;

				RayTracingShaderTable table(ray_traced_ambient_occlusion.Get());
				table.SetRayGenShader("RTAO_RayGen");
				table.AddMissShader("RTAO_Miss", 0);
				table.AddHitGroup("RTAOAnyHitGroup", 0);
				table.Commit(*gfx->GetDynamicAllocator(), dispatch_desc);
				cmd_list->DispatchRays(&dispatch_desc);
			}, ERGPassType::Compute, ERGPassFlags::None);

		blur_pass.AddPass(rg, RG_RES_NAME(RTAO_Output), RG_RES_NAME(AmbientOcclusion));
		AddGUI([&]() 
		{
			if (ImGui::TreeNodeEx("RTAO", ImGuiTreeNodeFlags_OpenOnDoubleClick))
			{
				ImGui::SliderFloat("Radius", &ao_radius, 1.0f, 16.0f);
				ImGui::TreePop();
				ImGui::Separator();
			}
		}
		);
	}

	void RayTracer::AddPathTracingPass(RenderGraph& rg)
	{
		if (!IsFeatureSupported(ERayTracingFeature::PathTracing)) return;

		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		struct PathTracingPassData
		{
			RGTextureReadWriteId output;
			RGTextureReadWriteId accumulation;

			RGBufferReadOnlyId vb;
			RGBufferReadOnlyId ib;
			RGBufferReadOnlyId geo;
		};

		rg.ImportBuffer(RG_RES_NAME(BigVertexBuffer), global_vb.get());
		rg.ImportBuffer(RG_RES_NAME(BigIndexBuffer), global_ib.get());
		rg.ImportBuffer(RG_RES_NAME(BigGeometryBuffer), geo_buffer.get());
		rg.ImportTexture(RG_RES_NAME(AccumulationTexture), accumulation_texture.get());

		rg.AddPass<PathTracingPassData>("Path Tracing Pass",
			[=](PathTracingPassData& data, RGBuilder& builder)
			{
				RGTextureDesc render_target_desc{};
				render_target_desc.format = EFormat::R16G16B16A16_FLOAT;
				render_target_desc.width = width;
				render_target_desc.height = height;
				render_target_desc.clear_value = ClearValue(0.0f, 0.0f, 0.0f, 0.0f);
				builder.DeclareTexture(RG_RES_NAME(PT_Output), render_target_desc);

				data.output = builder.WriteTexture(RG_RES_NAME(PT_Output));
				data.accumulation = builder.WriteTexture(RG_RES_NAME(AccumulationTexture));

				data.vb = builder.ReadBuffer(RG_RES_NAME(BigVertexBuffer));
				data.ib = builder.ReadBuffer(RG_RES_NAME(BigIndexBuffer));
				data.geo = builder.ReadBuffer(RG_RES_NAME(BigGeometryBuffer));
			},
			[=](PathTracingPassData const& data, RenderGraphContext& ctx, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				auto device = gfx->GetDevice();
				auto descriptor_allocator = gfx->GetOnlineDescriptorAllocator();

				uint32 i = (uint32)descriptor_allocator->AllocateRange(5);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 0), ctx.GetReadWriteTexture(data.accumulation), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 1), ctx.GetReadWriteTexture(data.output), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 2), ctx.GetReadOnlyBuffer(data.vb), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 3), ctx.GetReadOnlyBuffer(data.ib), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				device->CopyDescriptorsSimple(1, descriptor_allocator->GetHandle(i + 4), ctx.GetReadOnlyBuffer(data.geo), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				struct PathTracingConstants
				{
					int32   bounce_count;
					int32   accumulated_frames;
					uint32  accum_idx;
					uint32  output_idx;
					uint32  vertices_idx;
					uint32  indices_idx;
					uint32  geo_infos_idx;
				} constants =
				{
					.bounce_count = max_bounces, .accumulated_frames = accumulated_frames,
					.accum_idx = i + 0, .output_idx = i + 1,
					.vertices_idx = i + 2, .indices_idx = i + 3, .geo_infos_idx = i + 4
				};

				cmd_list->SetComputeRootSignature(RootSignatureCache::Get(ERootSignature::Common));
				cmd_list->SetComputeRootConstantBufferView(0, global_data.frame_cbuffer_address);
				cmd_list->SetComputeRoot32BitConstants(1, 7, &constants, 0);
				cmd_list->SetPipelineState1(path_tracing.Get());

				D3D12_DISPATCH_RAYS_DESC dispatch_desc{};
				dispatch_desc.Width = width;
				dispatch_desc.Height = height;
				dispatch_desc.Depth = 1;

				RayTracingShaderTable table(path_tracing.Get());
				table.SetRayGenShader("PT_RayGen");
				table.AddMissShader("PT_Miss", 0);
				table.AddHitGroup("PT_HitGroup", 0);
				table.Commit(*gfx->GetDynamicAllocator(), dispatch_desc);
				cmd_list->DispatchRays(&dispatch_desc);

			}, ERGPassType::Compute, ERGPassFlags::None); 

		++accumulated_frames;
		AddGUI([&]()
			{
				if (ImGui::TreeNodeEx("Path tracing", ImGuiTreeNodeFlags_OpenOnDoubleClick))
				{
					ImGui::SliderInt("Max bounces", &max_bounces, 1, 8);
					ImGui::TreePop();
					ImGui::Separator();
				}
			}
		);

	}

	void RayTracer::CreateStateObjects()
	{
		ID3D12Device5* device = gfx->GetDevice();

		Shader const& rt_shadows_blob		= ShaderCache::GetShader(LIB_Shadows);
		Shader const& rt_soft_shadows_blob	= ShaderCache::GetShader(LIB_SoftShadows);
		Shader const& rtao_blob				= ShaderCache::GetShader(LIB_AmbientOcclusion);
		Shader const& rtr_blob				= ShaderCache::GetShader(LIB_Reflections);
		Shader const& pt_blob				= ShaderCache::GetShader(LIB_PathTracing);

		StateObjectBuilder rt_shadows_state_object_builder(6);
		{
			D3D12_EXPORT_DESC export_descs[] =
			{
				D3D12_EXPORT_DESC{.Name = L"RTS_RayGen_Hard", .ExportToRename = L"RTS_RayGen"},
				D3D12_EXPORT_DESC{.Name = L"RTS_AnyHit", .ExportToRename = NULL},
				D3D12_EXPORT_DESC{.Name = L"RTS_Miss", .ExportToRename = NULL}
			};

			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rt_shadows_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rt_shadows_blob.GetPointer();
			dxil_lib_desc.NumExports = ARRAYSIZE(export_descs);
			dxil_lib_desc.pExports = export_descs;
			rt_shadows_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc2{};
			D3D12_EXPORT_DESC export_desc2{};
			export_desc2.ExportToRename = L"RTS_RayGen";
			export_desc2.Name = L"RTS_RayGen_Soft";
			dxil_lib_desc2.DXILLibrary.BytecodeLength = rt_soft_shadows_blob.GetLength();
			dxil_lib_desc2.DXILLibrary.pShaderBytecode = rt_soft_shadows_blob.GetPointer();
			dxil_lib_desc2.NumExports = 1;
			dxil_lib_desc2.pExports = &export_desc2;
			rt_shadows_state_object_builder.AddSubObject(dxil_lib_desc2);

			// Add a state subobject for the shader payload configuration
			D3D12_RAYTRACING_SHADER_CONFIG rt_shadows_shader_config{};
			rt_shadows_shader_config.MaxPayloadSizeInBytes = 4;	//bool in hlsl is 4 bytes
			rt_shadows_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rt_shadows_state_object_builder.AddSubObject(rt_shadows_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = RootSignatureCache::Get(ERootSignature::Common);
			rt_shadows_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config = {};
			pipeline_config.MaxTraceRecursionDepth = 1;
			rt_shadows_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC anyhit_group{};
			anyhit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			anyhit_group.AnyHitShaderImport = L"RTS_AnyHit";
			anyhit_group.HitGroupExport = L"ShadowAnyHitGroup";
			rt_shadows_state_object_builder.AddSubObject(anyhit_group);

			ray_traced_shadows.Attach(rt_shadows_state_object_builder.CreateStateObject(device));
		}

		StateObjectBuilder rtao_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rtao_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rtao_blob.GetPointer();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			rtao_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG rtao_shader_config{};
			rtao_shader_config.MaxPayloadSizeInBytes = 4;	//bool in hlsl is 4 bytes
			rtao_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rtao_state_object_builder.AddSubObject(rtao_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = RootSignatureCache::Get(ERootSignature::Common);
			rtao_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 1;
			rtao_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC anyhit_group{};
			anyhit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			anyhit_group.AnyHitShaderImport = L"RTAO_AnyHit";
			anyhit_group.HitGroupExport = L"RTAOAnyHitGroup";
			rtao_state_object_builder.AddSubObject(anyhit_group);

			ray_traced_ambient_occlusion.Attach(rtao_state_object_builder.CreateStateObject(device));
		}

		StateObjectBuilder rtr_state_object_builder(6);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = rtr_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = rtr_blob.GetPointer();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			rtr_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG rtr_shader_config{};
			rtr_shader_config.MaxPayloadSizeInBytes = sizeof(float) * 5;
			rtr_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			rtr_state_object_builder.AddSubObject(rtr_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = RootSignatureCache::Get(ERootSignature::Common);
			rtr_state_object_builder.AddSubObject(global_root_sig);

			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 2;
			rtr_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC closesthit_group{};
			closesthit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			closesthit_group.ClosestHitShaderImport = L"RTR_ClosestHitPrimaryRay";
			closesthit_group.HitGroupExport = L"RTRClosestHitGroupPrimaryRay";
			rtr_state_object_builder.AddSubObject(closesthit_group);

			closesthit_group.ClosestHitShaderImport = L"RTR_ClosestHitReflectionRay";
			closesthit_group.HitGroupExport = L"RTRClosestHitGroupReflectionRay";
			rtr_state_object_builder.AddSubObject(closesthit_group);

			ray_traced_reflections.Attach(rtr_state_object_builder.CreateStateObject(device));
		}

		StateObjectBuilder pt_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = pt_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = pt_blob.GetPointer();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			pt_state_object_builder.AddSubObject(dxil_lib_desc);
		
			D3D12_RAYTRACING_SHADER_CONFIG pt_shader_config{};
			pt_shader_config.MaxPayloadSizeInBytes = sizeof(float) * 15;
			pt_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			pt_state_object_builder.AddSubObject(pt_shader_config);
		
			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = RootSignatureCache::Get(ERootSignature::Common);
			pt_state_object_builder.AddSubObject(global_root_sig);
		
			// Add a state subobject for the ray tracing pipeline config
			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 3;
			pt_state_object_builder.AddSubObject(pipeline_config);
		
			D3D12_HIT_GROUP_DESC closesthit_group{};
			closesthit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			closesthit_group.ClosestHitShaderImport = L"PT_ClosestHit";
			closesthit_group.HitGroupExport = L"PT_HitGroup";
			pt_state_object_builder.AddSubObject(closesthit_group);
		
			path_tracing = pt_state_object_builder.CreateStateObject(device);
		}
	}
	void RayTracer::OnLibraryRecompiled(EShaderId shader)
	{
		CreateStateObjects();
	}
}
