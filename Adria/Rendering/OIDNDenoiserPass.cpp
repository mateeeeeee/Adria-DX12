#include "OIDNDenoiserPass.h"
#include "Logging/Logger.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"

#pragma comment(lib, "OpenImageDenoise.lib")
#pragma comment(lib, "OpenImageDenoise_core.lib")

namespace adria
{
	static void OIDNErrorCallback(void* ptr, OIDNError code, const char* message)
	{
		static Char const* code_names[] =
		{
			"None",
			"Unknown",
			"Invalid Argument",
			"Invalid Operation",
			"Out Of Memory",
			"Unsupported Hardware",
			"Cancelled"
		};
		ADRIA_LOG(ERROR, "%s : %s", code_names[code], message);
	}

	OIDNDenoiserPass::OIDNDenoiserPass(GfxDevice* gfx) : gfx(gfx)
	{
		oidn_device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
		if (!oidn_device)
		{
			Char const* msg = new Char[64];
			OIDNError error = oidnGetDeviceError(nullptr, &msg);
			OIDNErrorCallback(nullptr, error, msg);
			ADRIA_LOG(WARNING, "%s", msg);
			delete msg;
			return;
		}
		if ((oidnGetDeviceInt(oidn_device, "type") == OIDN_DEVICE_TYPE_CPU))
		{
			ADRIA_LOG(WARNING, "Only GPU devices are supported for OIDN denoiser!");
			return;
		}

		oidnSetDeviceErrorFunction(oidn_device, OIDNErrorCallback, nullptr);
		oidnCommitDevice(oidn_device);
		oidn_filter = oidnNewFilter(oidn_device, "RT");
		oidnCommitFilter(oidn_filter);

		oidn_fence.Create(gfx, "OIDN Fence");
		supported = true;
	}

	OIDNDenoiserPass::~OIDNDenoiserPass()
	{
		gfx->WaitForGPU();
		ReleaseBuffers();
		oidnReleaseFilter(oidn_filter);
		oidnReleaseDevice(oidn_device);
	}

	void OIDNDenoiserPass::AddPass(RenderGraph& rg)
	{
		if (!supported) return;
		RG_SCOPE(rg, "OIDN Denoiser");

		struct OIDNDenoiserPassData
		{
			RGTextureReadWriteId color;
			RGTextureReadOnlyId albedo;
			RGTextureReadOnlyId normal;
		};

		rg.AddPass<OIDNDenoiserPassData>("OIDN Denoiser Pass",
			[=](OIDNDenoiserPassData& data, RenderGraphBuilder& builder)
			{
				data.color = builder.WriteTexture(RG_NAME(PT_Output));
				data.albedo = builder.ReadTexture(RG_NAME(PT_Albedo));
				data.normal = builder.ReadTexture(RG_NAME(PT_Normal));
			},
			[&](OIDNDenoiserPassData const& data, RenderGraphContext& ctx, GfxCommandList* cmd_list)
			{
				GfxTexture& color  = ctx.GetTexture(*data.color);
				GfxTexture const& albedo = ctx.GetTexture(*data.albedo);
				GfxTexture const& normal = ctx.GetTexture(*data.normal);
				Denoise(cmd_list, color, albedo, normal);
			}, RGPassType::Compute);
	}

	void OIDNDenoiserPass::Reset()
	{
		denoised = false;
	}

	void OIDNDenoiserPass::CreateBuffers(GfxTexture& color_texture, GfxTexture const& albedo_texture, GfxTexture const& normal_texture)
	{
		Uint32 width = color_texture.GetWidth();
		Uint32 height = color_texture.GetHeight();
		if (oidn_color_buffer == nullptr || oidnGetBufferSize(oidn_color_buffer) != gfx->GetLinearBufferSize(&color_texture))
		{
			ReleaseBuffers();

			GfxBufferDesc oidn_buffer_desc{};
			oidn_buffer_desc.size = gfx->GetLinearBufferSize(&color_texture); 
			oidn_buffer_desc.resource_usage = GfxResourceUsage::Default;
			oidn_buffer_desc.bind_flags = GfxBindFlag::None;
			oidn_buffer_desc.misc_flags = GfxBufferMiscFlag::Shared;

			color_buffer = std::make_unique<GfxBuffer>(gfx, oidn_buffer_desc);
			albedo_buffer = std::make_unique<GfxBuffer>(gfx, oidn_buffer_desc);
			normal_buffer = std::make_unique<GfxBuffer>(gfx, oidn_buffer_desc);

			OIDNExternalMemoryTypeFlag flag = OIDN_EXTERNAL_MEMORY_TYPE_FLAG_D3D12_RESOURCE;
			oidn_color_buffer = oidnNewSharedBufferFromWin32Handle(oidn_device, flag, color_buffer->GetSharedHandle(), nullptr, oidn_buffer_desc.size);
			oidn_albedo_buffer = oidnNewSharedBufferFromWin32Handle(oidn_device, flag, albedo_buffer->GetSharedHandle(), nullptr, oidn_buffer_desc.size);
			oidn_normal_buffer = oidnNewSharedBufferFromWin32Handle(oidn_device, flag, normal_buffer->GetSharedHandle(), nullptr, oidn_buffer_desc.size);

			oidnSetFilterImage(oidn_filter, "color", oidn_color_buffer, OIDN_FORMAT_HALF3, width, height, 0, 8, color_texture.GetRowPitch());
			oidnSetFilterImage(oidn_filter, "albedo", oidn_albedo_buffer, OIDN_FORMAT_HALF3, width, height, 0, 8, albedo_texture.GetRowPitch());
			oidnSetFilterImage(oidn_filter, "normal", oidn_normal_buffer, OIDN_FORMAT_HALF3, width, height, 0, 8, normal_texture.GetRowPitch());
			oidnSetFilterBool(oidn_filter, "hdr", true);
			oidnSetFilterBool(oidn_filter, "cleanAux", true);
			oidnCommitFilter(oidn_filter);
		}
	}

	void OIDNDenoiserPass::ReleaseBuffers()
	{
		if (oidn_color_buffer)  oidnReleaseBuffer(oidn_color_buffer);
		if (oidn_albedo_buffer) oidnReleaseBuffer(oidn_albedo_buffer);
		if (oidn_normal_buffer) oidnReleaseBuffer(oidn_normal_buffer);
	}

	void OIDNDenoiserPass::Denoise(GfxCommandList* cmd_list, GfxTexture& color_texture, GfxTexture const& albedo_texture, GfxTexture const& normal_texture)
	{
		CreateBuffers(color_texture, albedo_texture, normal_texture);
		if (!denoised)
		{
			denoised = true;
			cmd_list->TextureBarrier(color_texture, GfxResourceState::CopyDst, GfxResourceState::CopySrc);
			cmd_list->CopyTextureToBuffer(*color_buffer, 0, color_texture, 0, 0);
			cmd_list->CopyTextureToBuffer(*albedo_buffer, 0, albedo_texture, 0, 0);
			cmd_list->CopyTextureToBuffer(*normal_buffer, 0, normal_texture, 0, 0);
			cmd_list->End();
			cmd_list->Signal(oidn_fence, ++oidn_fence_value);
			cmd_list->Submit();
			oidn_fence.Wait(oidn_fence_value);
			oidnExecuteFilter(oidn_filter);
			cmd_list->Begin();
			cmd_list->TextureBarrier(color_texture, GfxResourceState::CopySrc, GfxResourceState::CopyDst);
		}
		cmd_list->CopyBufferToTexture(color_texture, 0, 0, *color_buffer, 0);
	}

}
