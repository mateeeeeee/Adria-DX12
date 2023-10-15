#include "DDGI.h"
#include "BlackboardData.h"
#include "Components.h"
#include "ShaderStructs.h"
#include "ShaderCache.h"
#include "PSOCache.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraph.h"
#include "Math/Constants.h"
#include "Editor/GUICommand.h"
#include "Utilities/Random.h"
#include "entt/entity/registry.hpp"

namespace adria
{
	Vector2u DDGI::ProbeTextureDimensions(Vector3u const& num_probes, uint32 texels_per_probe)
	{
		uint32 width = (1 + texels_per_probe + 1) * num_probes.y * num_probes.x;
		uint32 height = (1 + texels_per_probe + 1) * num_probes.z;
		return Vector2u(width, height);
	}

	DDGI::DDGI(GfxDevice* gfx, entt::registry& reg, uint32 w, uint32 h) : gfx(gfx), reg(reg), width(w), height(h)
	{
		is_supported = gfx->GetCapabilities().SupportsRayTracing();
		if (IsSupported())
		{
			CreateStateObject();
			ShaderCache::GetLibraryRecompiledEvent().AddMember(&DDGI::OnLibraryRecompiled, *this);
		}
	}

	void DDGI::OnSceneInitialized()
	{
		BoundingBox scene_bounding_box;
		for (auto mesh_entity : reg.view<Mesh>())
		{
			Mesh& mesh = reg.get<Mesh>(mesh_entity);

			for (auto const& instance : mesh.instances)
			{
				SubMeshGPU& submesh = mesh.submeshes[instance.submesh_index];
				BoundingBox instance_bounding_box;
				submesh.bounding_box.Transform(instance_bounding_box, instance.world_transform);
				BoundingBox::CreateMerged(scene_bounding_box, scene_bounding_box, instance_bounding_box);
			}
		}
		ddgi_volume.origin = scene_bounding_box.Center;
		ddgi_volume.extents = 1.1f * Vector3(scene_bounding_box.Extents);
		ddgi_volume.num_probes = Vector3u(16, 12, 14);
		ddgi_volume.num_rays = 128;
		ddgi_volume.max_num_rays = 512;

		Vector2u irradiance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_IRRADIANCE_TEXELS);
		GfxTextureDesc irradiance_desc{};
		irradiance_desc.width = irradiance_dimensions.x;
		irradiance_desc.height = irradiance_dimensions.y;
		irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
		irradiance_desc.bind_flags = GfxBindFlag::ShaderResource;
		irradiance_desc.initial_state = GfxResourceState::CopyDest;
		ddgi_volume.irradiance_history = gfx->CreateTexture(irradiance_desc);
		ddgi_volume.irradiance_history->SetName("DDGI Irradiance History");

		ddgi_volume.irradiance_history_srv = gfx->CreateTextureSRV(ddgi_volume.irradiance_history.get());

		Vector2u distance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_DISTANCE_TEXELS);
		GfxTextureDesc distance_desc{};
		distance_desc.width = distance_dimensions.x;
		distance_desc.height = distance_dimensions.y;
		distance_desc.format = GfxFormat::R16G16_FLOAT;
		distance_desc.bind_flags = GfxBindFlag::ShaderResource;
		distance_desc.initial_state = GfxResourceState::CopyDest;
		ddgi_volume.distance_history = gfx->CreateTexture(distance_desc);
		ddgi_volume.distance_history->SetName("DDGI Distance History");

		ddgi_volume.distance_history_srv = gfx->CreateTextureSRV(ddgi_volume.distance_history.get());
	}

	void DDGI::OnResize(uint32 w, uint32 h)
	{
		if (!IsSupported()) return;
		width = w, height = h;
	}

	void DDGI::AddPasses(RenderGraph& rg)
	{
		if (!IsSupported()) return;

		GUI_RunCommand([&]()
			{
				if (ImGui::TreeNode("DDGI"))
				{
					ImGui::Checkbox("Enable DDGI", &enabled);
					ImGui::TreePop();
				}
			}
		);
		GUI_DisplayTexture("DDGI Irradiance", ddgi_volume.irradiance_history.get());
		GUI_DisplayTexture("DDGI Distance", ddgi_volume.distance_history.get());

		if (!enabled) return;

		uint32 const num_probes_flat = ddgi_volume.num_probes.x * ddgi_volume.num_probes.y * ddgi_volume.num_probes.z;

		RealRandomGenerator rng(0.0f, 1.0f);
		Vector3 random_vector(2.0f * rng() - 1.0f, 2.0f * rng() - 1.0f, 2.0f * rng() - 1.0f); random_vector.Normalize();
		float random_angle = rng() * pi<float> * 2.0f;

		FrameBlackboardData const& global_data = rg.GetBlackboard().Get<FrameBlackboardData>();
		rg.ImportTexture(RG_RES_NAME(DDGIIrradianceHistory), ddgi_volume.irradiance_history.get());
		rg.ImportTexture(RG_RES_NAME(DDGIDistanceHistory), ddgi_volume.distance_history.get());

		struct DDGIBlackboardData
		{
			uint32 heap_index;
		};

		struct DDGIRayTracePassData
		{
			RGBufferReadWriteId ray_buffer;
			RGTextureReadOnlyId irradiance_history;
			RGTextureReadOnlyId distance_history;
		};

		rg.AddPass<DDGIRayTracePassData>("DDGI Ray Trace Pass",
			[=](DDGIRayTracePassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc ray_buffer_desc{};
				ray_buffer_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				ray_buffer_desc.stride = GetGfxFormatStride(ray_buffer_desc.format);
				ray_buffer_desc.size = ray_buffer_desc.stride * num_probes_flat * ddgi_volume.max_num_rays;
				builder.DeclareBuffer(RG_RES_NAME(DDGIRayBuffer), ray_buffer_desc);

				data.ray_buffer = builder.WriteBuffer(RG_RES_NAME(DDGIRayBuffer));
				data.irradiance_history = builder.ReadTexture(RG_RES_NAME(DDGIIrradianceHistory));
				data.distance_history = builder.ReadTexture(RG_RES_NAME(DDGIDistanceHistory));
			},
			[=](DDGIRayTracePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				uint32 i = gfx->AllocateDescriptorsGPU(1).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), ctx.GetReadWriteBuffer(data.ray_buffer));
				ctx.GetBlackboard().Create<DDGIBlackboardData>(i);

				struct DDGIParameters
				{
					Vector3  random_vector;
					float    random_angle;
					float    history_blend_weight;
					uint32   ray_buffer_index;
				} parameters
				{
					.random_vector = random_vector,
					.random_angle = random_angle,
					.history_blend_weight = 0.98f,
					.ray_buffer_index = i
				};

				GfxRayTracingShaderTable& table = cmd_list->SetStateObject(ddgi_trace_so.Get());
				table.SetRayGenShader("DDGI_RayGen");
				table.AddMissShader("DDGI_Miss", 0);
				table.AddHitGroup("DDGI_HitGroup", 0);

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				
				cmd_list->DispatchRays(ddgi_volume.num_rays, num_probes_flat);
				cmd_list->UavBarrier(ctx.GetBuffer(*data.ray_buffer));

			}, RGPassType::Compute);

		struct DDGIUpdateIrradiancePassData
		{
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		irradiance_history;
			RGTextureReadWriteId	irradiance;
		};

		rg.AddPass<DDGIUpdateIrradiancePassData>("DDGI Update Irradiance Pass",
			[=](DDGIUpdateIrradiancePassData& data, RenderGraphBuilder& builder)
			{
				Vector2u irradiance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_IRRADIANCE_TEXELS);
				RGTextureDesc irradiance_desc{};
				irradiance_desc.width  = irradiance_dimensions.x;
				irradiance_desc.height = irradiance_dimensions.y;
				irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(DDGIIrradiance), irradiance_desc);

				data.irradiance		= builder.WriteTexture(RG_RES_NAME(DDGIIrradiance));
				data.ray_buffer		= builder.ReadBuffer(RG_RES_NAME(DDGIRayBuffer));
				data.irradiance_history = builder.ReadTexture(RG_RES_NAME(DDGIIrradianceHistory));
			},
			[=](DDGIUpdateIrradiancePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{
				GfxDevice* gfx = cmd_list->GetDevice();
				DDGIBlackboardData const& ddgi_blackboard = ctx.GetBlackboard().Get<DDGIBlackboardData>();

				uint32 i = gfx->AllocateDescriptorsGPU(1).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), ctx.GetReadWriteTexture(data.irradiance));

				struct DDGIParameters
				{
					Vector3  random_vector;
					float    random_angle;
					float    history_blend_weight;
					uint32   ray_buffer_index;
					uint32   irradiance_idx;
				} parameters
				{
					.random_vector = random_vector,
					.random_angle = random_angle,
					.history_blend_weight = 0.98f,
					.ray_buffer_index = ddgi_blackboard.heap_index,
					.irradiance_idx = i
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateIrradiance));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch(num_probes_flat, 1, 1);
				cmd_list->UavBarrier(ctx.GetTexture(*data.irradiance));
			}, RGPassType::Compute);

		struct DDGIUpdateDistancePassData
		{
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		distance_history;
			RGTextureReadWriteId	distance;
		};

		rg.AddPass<DDGIUpdateDistancePassData>("DDGI Update Distance Pass",
			[=](DDGIUpdateDistancePassData& data, RenderGraphBuilder& builder)
			{
				Vector2u distance_dimensions = ProbeTextureDimensions(ddgi_volume.num_probes, PROBE_DISTANCE_TEXELS);
				RGTextureDesc distance_desc{};
				distance_desc.width = distance_dimensions.x;
				distance_desc.height = distance_dimensions.y;
				distance_desc.format = GfxFormat::R16G16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(DDGIDistance), distance_desc);

				data.distance = builder.WriteTexture(RG_RES_NAME(DDGIDistance));
				data.ray_buffer = builder.ReadBuffer(RG_RES_NAME(DDGIRayBuffer));
				data.distance_history = builder.ReadTexture(RG_RES_NAME(DDGIDistanceHistory));
			},
			[=](DDGIUpdateDistancePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list) mutable
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				DDGIBlackboardData const& ddgi_blackboard = ctx.GetBlackboard().Get<DDGIBlackboardData>();

				uint32 i = gfx->AllocateDescriptorsGPU(1).GetIndex();
				gfx->CopyDescriptors(1, gfx->GetDescriptorGPU(i), ctx.GetReadWriteTexture(data.distance));

				struct DDGIParameters
				{
					Vector3  random_vector;
					float    random_angle;
					float    history_blend_weight;
					uint32   ray_buffer_index;
					uint32   distance_idx;
				} parameters
				{
					.random_vector = random_vector,
					.random_angle = random_angle,
					.history_blend_weight = 0.98f,
					.ray_buffer_index = ddgi_blackboard.heap_index,
					.distance_idx = i
				};

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateDistance));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch(num_probes_flat, 1, 1);
				cmd_list->UavBarrier(ctx.GetTexture(*data.distance));
			}, RGPassType::Compute);

		rg.ExportTexture(RG_RES_NAME(DDGIIrradiance), ddgi_volume.irradiance_history.get());
		rg.ExportTexture(RG_RES_NAME(DDGIDistance), ddgi_volume.distance_history.get());
	}

	int32 DDGI::GetDDGIVolumeIndex()
	{
		if (!IsSupported() || !enabled)  return -1;

		std::vector<DDGIVolumeHLSL> ddgi_data;
		DDGIVolumeHLSL& ddgi_hlsl = ddgi_data.emplace_back();
		ddgi_hlsl.start_position = ddgi_volume.origin - ddgi_volume.extents;
		ddgi_hlsl.probe_size = 2 * ddgi_volume.extents / (Vector3((float)ddgi_volume.num_probes.x, (float)ddgi_volume.num_probes.y, (float)ddgi_volume.num_probes.z) - Vector3::One);
		ddgi_hlsl.rays_per_probe = ddgi_volume.num_rays;
		ddgi_hlsl.max_rays_per_probe = ddgi_volume.max_num_rays;
		ddgi_hlsl.probe_count = Vector3i(ddgi_volume.num_probes.x, ddgi_volume.num_probes.y, ddgi_volume.num_probes.z);
		ddgi_hlsl.normal_bias = 0.25f;
		ddgi_hlsl.energy_preservation = 0.85f;

		GfxDescriptor irradiance_gpu = gfx->AllocateDescriptorsGPU();
		GfxDescriptor distance_gpu = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, irradiance_gpu, ddgi_volume.irradiance_history_srv);
		gfx->CopyDescriptors(1, distance_gpu, ddgi_volume.distance_history_srv);

		ddgi_hlsl.irradiance_history_idx = (int32)irradiance_gpu.GetIndex();
		ddgi_hlsl.distance_history_idx = (int32)distance_gpu.GetIndex();
		if (!ddgi_volume_buffer || ddgi_volume_buffer->GetCount() < ddgi_data.size())
		{
			ddgi_volume_buffer = gfx->CreateBuffer(StructuredBufferDesc<DDGIVolumeHLSL>(ddgi_data.size(), false, true));
			ddgi_volume_buffer_srv = gfx->CreateBufferSRV(ddgi_volume_buffer.get());
		}

		ddgi_volume_buffer->Update(ddgi_data.data(), ddgi_data.size() * sizeof(DDGIVolumeHLSL));
		GfxDescriptor ddgi_volume_buffer_srv_gpu = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, ddgi_volume_buffer_srv_gpu, ddgi_volume_buffer_srv);
		return (int32)ddgi_volume_buffer_srv_gpu.GetIndex();
	}

	void DDGI::CreateStateObject()
	{
		ID3D12Device5* device = gfx->GetDevice();
		GfxShader const& ddgi_blob = ShaderCache::GetShader(LIB_DDGIRayTracing);

		GfxStateObjectBuilder ddgi_state_object_builder(5);
		{
			D3D12_DXIL_LIBRARY_DESC	dxil_lib_desc{};
			dxil_lib_desc.DXILLibrary.BytecodeLength = ddgi_blob.GetLength();
			dxil_lib_desc.DXILLibrary.pShaderBytecode = ddgi_blob.GetPointer();
			dxil_lib_desc.NumExports = 0;
			dxil_lib_desc.pExports = nullptr;
			ddgi_state_object_builder.AddSubObject(dxil_lib_desc);

			D3D12_RAYTRACING_SHADER_CONFIG ddgi_shader_config{};
			ddgi_shader_config.MaxPayloadSizeInBytes = 16;
			ddgi_shader_config.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;
			ddgi_state_object_builder.AddSubObject(ddgi_shader_config);

			D3D12_GLOBAL_ROOT_SIGNATURE global_root_sig{};
			global_root_sig.pGlobalRootSignature = gfx->GetCommonRootSignature();
			ddgi_state_object_builder.AddSubObject(global_root_sig);

			D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_config{};
			pipeline_config.MaxTraceRecursionDepth = 1;
			ddgi_state_object_builder.AddSubObject(pipeline_config);

			D3D12_HIT_GROUP_DESC hit_group{};
			hit_group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
			hit_group.ClosestHitShaderImport = L"DDGI_ClosestHit";
			hit_group.HitGroupExport = L"DDGI_HitGroup";
			ddgi_state_object_builder.AddSubObject(hit_group);
		}
		ddgi_trace_so.Attach(ddgi_state_object_builder.CreateStateObject(device));
	}

	void DDGI::OnLibraryRecompiled(GfxShaderID shader)
	{
		if (shader == LIB_DDGIRayTracing) CreateStateObject();
	}
}

