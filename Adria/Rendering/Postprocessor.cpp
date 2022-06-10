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
	Postprocessor::Postprocessor(TextureManager& texture_manager, uint32 width, uint32 height)
		: texture_manager(texture_manager), width(width), height(height),
		  blur_pass(width, height), copy_to_texture_pass(width, height)
	{
	}

	void Postprocessor::AddPasses(RenderGraph& rg, PostprocessSettings const& settings)
	{
		AddCopyHDRPass(rg);
	}

	void Postprocessor::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
		blur_pass.OnResize(width, height);
		copy_to_texture_pass.OnResize(width, height);
	}

	void Postprocessor::OnSceneInitialized()
	{
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\weather.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\cloud.dds"));
		cloud_textures.push_back(texture_manager.LoadTexture(L"Resources\\Textures\\clouds\\worley.dds"));
	}

	RGResourceName Postprocessor::GetFinalResource() const
	{
		return RG_RES_NAME(PostprocessCopy);
	}

	void Postprocessor::AddCopyHDRPass(RenderGraph& rg)
	{
		struct CopyPassData
		{
			RGTextureCopySrcId copy_src;
			RGTextureCopyDstId copy_dst;
		};

		rg.AddPass<CopyPassData>("Copy HDR Pass",
			[=](CopyPassData& data, RenderGraphBuilder& builder)
			{
				TextureDesc postprocess_desc{};
				postprocess_desc.width = width;
				postprocess_desc.height = height;
				postprocess_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
				postprocess_desc.bind_flags = EBindFlag::UnorderedAccess | EBindFlag::RenderTarget | EBindFlag::ShaderResource;
				postprocess_desc.initial_state = D3D12_RESOURCE_STATE_COPY_DEST;

				builder.DeclareTexture(RG_RES_NAME(PostprocessCopy), postprocess_desc);
				data.copy_dst = builder.WriteCopyDstTexture(RG_RES_NAME(PostprocessCopy));
				data.copy_src = builder.ReadCopySrcTexture(RG_RES_NAME(HDR_RenderTarget));
			},
			[=](CopyPassData const& data, RenderGraphResources& resources, GraphicsDevice* gfx, RGCommandList* cmd_list)
			{
				Texture const& src_texture = resources.GetCopyResource(data.copy_src);
				Texture const& dst_texture = resources.GetCopyResource(data.copy_dst);
				cmd_list->CopyResource(dst_texture.GetNative(), src_texture.GetNative());
			}, ERGPassType::Copy, ERGPassFlags::None);
	}


}

