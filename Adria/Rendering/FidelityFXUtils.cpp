#include "FidelityFXUtils.h"
#include "Graphics/GfxFormat.h"
#include "Graphics/GfxTexture.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"


namespace adria
{
	static FfxSurfaceFormat GetFfxSurfaceFormat(GfxFormat format)
	{
		switch (format)
		{
		case (GfxFormat::R32G32B32A32_FLOAT):
			return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
		case (GfxFormat::R16G16B16A16_FLOAT):
			return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
		case (GfxFormat::R32G32_FLOAT):
			return FFX_SURFACE_FORMAT_R32G32_FLOAT;
		case (GfxFormat::R8_UINT):
			return FFX_SURFACE_FORMAT_R8_UINT;
		case (GfxFormat::R32_UINT):
			return FFX_SURFACE_FORMAT_R32_UINT;
		case (GfxFormat::R8G8B8A8_UNORM):
			return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
		case (GfxFormat::R8G8B8A8_UNORM_SRGB):
			return FFX_SURFACE_FORMAT_R8G8B8A8_SRGB;
		case (GfxFormat::R11G11B10_FLOAT):
			return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
		case (GfxFormat::R16G16_FLOAT):
			return FFX_SURFACE_FORMAT_R16G16_FLOAT;
		case (GfxFormat::R16G16_UINT):
			return FFX_SURFACE_FORMAT_R16G16_UINT;
		case (GfxFormat::R16_FLOAT):
			return FFX_SURFACE_FORMAT_R16_FLOAT;
		case (GfxFormat::R16_UINT):
			return FFX_SURFACE_FORMAT_R16_UINT;
		case (GfxFormat::R16_UNORM):
			return FFX_SURFACE_FORMAT_R16_UNORM;
		case (GfxFormat::R16_SNORM):
			return FFX_SURFACE_FORMAT_R16_SNORM;
		case (GfxFormat::R8_UNORM):
			return FFX_SURFACE_FORMAT_R8_UNORM;
		case GfxFormat::R8G8_UNORM:
			return FFX_SURFACE_FORMAT_R8G8_UNORM;
		case GfxFormat::R32_FLOAT:
		case GfxFormat::D32_FLOAT:
		case GfxFormat::R32_TYPELESS:
			return FFX_SURFACE_FORMAT_R32_FLOAT;
		case (GfxFormat::UNKNOWN):
			return FFX_SURFACE_FORMAT_UNKNOWN;
		case GfxFormat::R32G32B32A32_UINT:
			return FFX_SURFACE_FORMAT_R32G32B32A32_UINT;
		default:
			return FFX_SURFACE_FORMAT_UNKNOWN;
		}
	}
	static FfxResourceDescription GetFfxResourceDescription(GfxBuffer const& buffer, FfxResourceUsage additional_usage)
	{
		FfxResourceDescription resource_description{};
		GfxBufferDesc const& buffer_desc = buffer.GetDesc();
		resource_description.flags = FFX_RESOURCE_FLAGS_NONE;
		resource_description.usage = FFX_RESOURCE_USAGE_UAV;
		resource_description.width = (uint32)buffer_desc.size;
		resource_description.height = buffer_desc.stride;
		resource_description.format = FFX_SURFACE_FORMAT_UNKNOWN;
		resource_description.depth = 0;
		resource_description.mipCount = 0;
		resource_description.type = FFX_RESOURCE_TYPE_BUFFER;
		return resource_description;
	}
	static FfxResourceDescription GetFfxResourceDescription(GfxTexture const& texture, FfxResourceUsage additional_usage)
	{
		FfxResourceDescription resource_description{};
		GfxTextureDesc const& texture_desc = texture.GetDesc();

		resource_description.flags = FFX_RESOURCE_FLAGS_NONE;
		resource_description.usage = IsGfxFormatDepth(texture_desc.format) ? FFX_RESOURCE_USAGE_DEPTHTARGET : FFX_RESOURCE_USAGE_READ_ONLY;
		if (static_cast<bool>(texture_desc.bind_flags & GfxBindFlag::UnorderedAccess)) resource_description.usage = (FfxResourceUsage)(resource_description.usage | FFX_RESOURCE_USAGE_UAV);

		resource_description.width = texture_desc.width;
		resource_description.height = texture_desc.height;
		resource_description.depth = texture_desc.array_size;
		resource_description.mipCount = texture_desc.mip_levels;
		resource_description.format = GetFfxSurfaceFormat(texture_desc.format);
		resource_description.usage = (FfxResourceUsage)(resource_description.usage | additional_usage);

		switch (texture_desc.type)
		{
		case GfxTextureType_1D:
			resource_description.type = FFX_RESOURCE_TYPE_TEXTURE1D;
			break;
		case GfxTextureType_2D:
			if (texture_desc.misc_flags == GfxTextureMiscFlag::TextureCube)
			{
				resource_description.type = FFX_RESOURCE_TYPE_TEXTURE_CUBE;
			}
			else
			{
				resource_description.type = FFX_RESOURCE_TYPE_TEXTURE2D;
			}
			break;
		case GfxTextureType_3D:
			resource_description.type = FFX_RESOURCE_TYPE_TEXTURE3D;
			break;
		default:
			ADRIA_ASSERT(false);
			break;
		}
		return resource_description;
	}

	FfxInterface* CreateFfxInterface(GfxDevice* gfx, uint32 context_count)
	{
		FfxInterface* ffx_interface = new FfxInterface{};
		uint64 const scratch_buffer_size = ffxGetScratchMemorySizeDX12(context_count);
		void* scratch_buffer = malloc(scratch_buffer_size);
		ADRIA_ASSERT(scratch_buffer);
		memset(scratch_buffer, 0, scratch_buffer_size);
		FfxErrorCode error_code = ffxGetInterfaceDX12(ffx_interface, gfx->GetDevice(), scratch_buffer, scratch_buffer_size, context_count);
		ADRIA_ASSERT(error_code == FFX_OK);
		return ffx_interface;
	}

	void DestroyFfxInterface(FfxInterface* ffx_interface)
	{
		if (ffx_interface && ffx_interface->scratchBuffer)
		{
			free(ffx_interface->scratchBuffer);
			ffx_interface->scratchBuffer = nullptr;
		}
	}

	FfxResource GetFfxResource()
	{
		return ffxGetResourceDX12(nullptr, FfxResourceDescription{}, L"");
	}
	FfxResource GetFfxResource(GfxBuffer const& buffer, FfxResourceStates state, FfxResourceUsage additional_usage)
	{
		return ffxGetResourceDX12(buffer.GetNative(), GetFfxResourceDescription(buffer, additional_usage), L"", state);
	}
	FfxResource GetFfxResource(GfxTexture const& texture, FfxResourceStates state, FfxResourceUsage additional_usage)
	{
		return ffxGetResourceDX12(texture.GetNative(), GetFfxResourceDescription(texture, additional_usage), L"", state);
	}

}
