#include "Postprocessor.h"
#include "ConstantBuffers.h"
#include "Components.h"
#include "GlobalBlackboardData.h"
#include "RootSigPSOManager.h"
#include "../RenderGraph/RenderGraph.h"
#include "../Graphics/GPUProfiler.h"
#include "../tecs/registry.h"

namespace adria
{

	Postprocessor::CopyHDRPassData const& Postprocessor::AddCopyHDRPass(RenderGraph& rg, RGTextureRef hdr_texture)
	{
		GlobalBlackboardData const& global_data = rg.GetBlackboard().GetChecked<GlobalBlackboardData>();
		return rg.AddPass<CopyHDRPassData>("Copy HDR Pass",
			[=](CopyHDRPassData& data, RenderGraphBuilder& builder)
			{
				D3D12_CLEAR_VALUE rtv_clear_value{};
				rtv_clear_value.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				rtv_clear_value.Color[0] = 0.0f;
				rtv_clear_value.Color[1] = 0.0f;
				rtv_clear_value.Color[2] = 0.0f;
				rtv_clear_value.Color[3] = 0.0f;

				TextureDesc postprocess_desc{};
				postprocess_desc.clear = rtv_clear_value;
				postprocess_desc.width = width;
				postprocess_desc.height = height;
				postprocess_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				postprocess_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				postprocess_desc.initial_state = D3D12_RESOURCE_STATE_COPY_DEST;

				RGTextureRef postprocess = builder.CreateTexture("Postprocess Texture", postprocess_desc);
				data.src_texture = builder.Read(hdr_texture);
				data.dst_texture = builder.Write(postprocess);
			},
			[=](CopyHDRPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, CommandList* cmd_list)
			{
				Texture& src_texture = resources.GetTexture(data.src_texture);
				Texture& dst_texture = resources.GetTexture(data.dst_texture);
				cmd_list->CopyResource(dst_texture.GetNative(), src_texture.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::None);

	}

}

