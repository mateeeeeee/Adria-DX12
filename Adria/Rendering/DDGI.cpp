#include "DDGI.h"
#include "BlackboardData.h"
#include "ShaderCache.h"
#include "PSOCache.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxShader.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraph.h"
#include "Editor/GUICommand.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	XMUINT2 DDGI::ProbeTextureDimensions(XMUINT3 const& num_probes, uint32 texels_per_probe)
	{
		uint32 width = (1 + texels_per_probe + 1) * num_probes.y * num_probes.x;
		uint32 height = (1 + texels_per_probe + 1) * num_probes.z;
		return XMUINT2(width, height);
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

	void DDGI::OnResize(uint32 w, uint32 h)
	{
		if (!IsSupported()) return;
		width = w, height = h;
	}

	void DDGI::AddPasses(RenderGraph& rg)
	{
		if (ddgi_volumes.empty()) return;

		uint64 random_index = 0;
		DDGIVolume& ddgi = ddgi_volumes[random_index];
		uint32 const num_probes = ddgi.num_probes.x * ddgi.num_probes.y * ddgi.num_probes.z;
		uint32 const ddgi_num_rays = ddgi.num_rays;
		uint32 const ddgi_max_num_rays = ddgi.max_num_rays;
		XMUINT3 ddgi_num_probes = ddgi.num_probes;

		struct DDGIParameters
		{
			XMFLOAT3 random_vector;
			float    random_angle;
			float    history_blend_weight;
			uint32   volume_index;
		} parameters
		{
			.random_vector = XMFLOAT3(),
			.random_angle = 0.0f,
			.history_blend_weight = 0.98f,
			.volume_index = random_index
		};

		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();

		rg.ImportBuffer(RG_RES_NAME(ProbeStates), ddgi.probe_states.get());
		rg.ImportBuffer(RG_RES_NAME(ProbeOffsets), ddgi.probe_offsets.get());
		rg.ImportTexture(RG_RES_NAME(IrradianceHistory), ddgi.irradiance_history.get());
		rg.ImportTexture(RG_RES_NAME(DepthHistory), ddgi.depth_history.get());

		struct DDGIRayTracePassData
		{
			RGBufferReadOnlyId  probe_states;
			RGBufferReadWriteId ray_buffer;
		};

		rg.AddPass<DDGIRayTracePassData>("DDGI Ray Trace Pass",
			[=](DDGIRayTracePassData& data, RenderGraphBuilder& builder)
			{
				RGBufferDesc ray_buffer_desc{};
				ray_buffer_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				ray_buffer_desc.stride = GetGfxFormatStride(ray_buffer_desc.format);
				ray_buffer_desc.size = ray_buffer_desc.stride * num_probes * ddgi_max_num_rays;
				builder.DeclareBuffer(RG_RES_NAME(RayBuffer), ray_buffer_desc);
				data.probe_states = builder.ReadBuffer(RG_RES_NAME(ProbeStates));
				data.ray_buffer	  = builder.WriteBuffer(RG_RES_NAME(RayBuffer));
			},
			[=](DDGIRayTracePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				auto& table = cmd_list->SetStateObject(ddgi_trace_so.Get());
				table.SetRayGenShader("DDGI_RayGen");
				table.AddMissShader("DDGI_Miss", 0);
				table.AddHitGroup("DDGIHitGroup", 0);

				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->DispatchRays(ddgi_num_rays, num_probes);
				cmd_list->UavBarrier(ctx.GetBuffer(*data.ray_buffer));

			}, RGPassType::Compute);

		struct DDGIUpdateIrradiancePassData
		{
			RGBufferReadOnlyId		probe_states;
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		irradiance_history;
			RGTextureReadWriteId	irradiance;
		};

		rg.AddPass<DDGIUpdateIrradiancePassData>("DDGI Update Irradiance Pass",
			[=](DDGIUpdateIrradiancePassData& data, RenderGraphBuilder& builder)
			{
				XMUINT2 irradiance_dimensions = ProbeTextureDimensions(ddgi_num_probes, PROBE_IRRADIANCE_TEXELS);
				RGTextureDesc irradiance_desc{};
				irradiance_desc.width  = irradiance_dimensions.x;
				irradiance_desc.height = irradiance_dimensions.y;
				irradiance_desc.format = GfxFormat::R16G16B16A16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(Irradiance), irradiance_desc);

				data.irradiance		= builder.WriteTexture(RG_RES_NAME(Irradiance));
				data.probe_states	= builder.ReadBuffer(RG_RES_NAME(ProbeStates));
				data.ray_buffer		= builder.ReadBuffer(RG_RES_NAME(RayBuffer));
				data.irradiance_history = builder.ReadTexture(RG_RES_NAME(IrradianceHistory));
			},
			[=](DDGIUpdateIrradiancePassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateIrradianceColor));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch(num_probes, 1, 1);
				cmd_list->UavBarrier(ctx.GetTexture(*data.irradiance));
			}, RGPassType::Compute);

		struct DDGIUpdateDepthPassData
		{
			RGBufferReadOnlyId		probe_states;
			RGBufferReadOnlyId		ray_buffer;
			RGTextureReadOnlyId		depth_history;
			RGTextureReadWriteId	depth;
		};

		rg.AddPass<DDGIUpdateDepthPassData>("DDGI Update Depth Pass",
			[=](DDGIUpdateDepthPassData& data, RenderGraphBuilder& builder)
			{
				XMUINT2 depth_dimensions = ProbeTextureDimensions(ddgi_num_probes, PROBE_DEPTH_TEXELS);
				RGTextureDesc irradiance_desc{};
				irradiance_desc.width = depth_dimensions.x;
				irradiance_desc.height = depth_dimensions.y;
				irradiance_desc.format = GfxFormat::R16G16_FLOAT;
				builder.DeclareTexture(RG_RES_NAME(Depth), irradiance_desc);

				data.depth = builder.WriteTexture(RG_RES_NAME(Depth));
				data.probe_states = builder.ReadBuffer(RG_RES_NAME(ProbeStates));
				data.ray_buffer = builder.ReadBuffer(RG_RES_NAME(RayBuffer));
				data.depth_history = builder.ReadTexture(RG_RES_NAME(IrradianceHistory));
			},
			[=](DDGIUpdateDepthPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateIrradianceDepth));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch(num_probes, 1, 1);
				cmd_list->UavBarrier(ctx.GetTexture(*data.depth));
			}, RGPassType::Compute);

		struct DDGIUpdateProbeStatesPassData
		{
			RGBufferReadWriteId		probe_states;
			RGBufferReadWriteId		probe_offsets;
			RGBufferReadOnlyId		ray_buffer;
		};
		
		rg.AddPass<DDGIUpdateProbeStatesPassData>("DDGI Update Probe States Pass",
			[=](DDGIUpdateProbeStatesPassData& data, RenderGraphBuilder& builder)
			{
				data.probe_states = builder.WriteBuffer(RG_RES_NAME(ProbeStates));
				data.probe_offsets = builder.WriteBuffer(RG_RES_NAME(ProbeOffsets));
				data.ray_buffer = builder.ReadBuffer(RG_RES_NAME(RayBuffer));
			},
			[=](DDGIUpdateProbeStatesPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxDevice* gfx = cmd_list->GetDevice();

				cmd_list->SetPipelineState(PSOCache::Get(GfxPipelineStateID::DDGIUpdateProbeStates));
				cmd_list->SetRootCBV(0, global_data.frame_cbuffer_address);
				cmd_list->SetRootConstants(1, parameters);
				cmd_list->Dispatch(num_probes / 32, 1, 1);
			}, RGPassType::Compute);


		//copy irradiance to history for next frame
	}

	void DDGI::CreateStateObject()
	{

	}

	void DDGI::OnLibraryRecompiled(GfxShaderID shader)
	{
		if (shader == LIB_AmbientOcclusion) CreateStateObject();
	}

}

