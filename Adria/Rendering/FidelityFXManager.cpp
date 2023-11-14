#include "FidelityFX/host/backends/dx12/ffx_dx12.h"
#include "FidelityFXManager.h"
#include "FFXDepthOfFieldPass.h"
#include "FSR2Pass.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	FidelityFXManager::FidelityFXManager(GfxDevice* gfx, uint32 width, uint32 height)
	{
		uint32 context_count = FFX_FSR2_CONTEXT_COUNT + FFX_DOF_CONTEXT_COUNT;

		uint64 const scratch_buffer_size = ffxGetScratchMemorySizeDX12(context_count);
		void* scratch_buffer = malloc(scratch_buffer_size);
		FfxErrorCode error_code = ffxGetInterfaceDX12(&ffx_interface, gfx->GetDevice(), scratch_buffer, scratch_buffer_size, context_count);
		ADRIA_ASSERT(error_code == FFX_OK);

		ffx_depth_of_field = std::make_unique<FFXDepthOfFieldPass>(gfx, ffx_interface, width, height);
		ffx_fsr2_pass = std::make_unique<FSR2Pass>(gfx, ffx_interface, width, height);
	}


	FidelityFXManager::~FidelityFXManager()
	{
		ffx_fsr2_pass.reset(nullptr);
		ffx_depth_of_field.reset(nullptr);
		if (ffx_interface.scratchBuffer)
		{
			free(ffx_interface.scratchBuffer);
			ffx_interface.scratchBuffer = nullptr;
		}
	}

}
