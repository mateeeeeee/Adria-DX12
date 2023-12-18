#include "FidelityFX/host/ffx_interface.h"
#include "FidelityFX/host/backends/dx12/ffx_dx12.h"
#include "FidelityFXManager.h"
#include "FFXDepthOfFieldPass.h"
#include "FFXCACAOPass.h"
#include "FSR2Pass.h"
#include "FSR3Pass.h"
#include "FFXCASPass.h"
#include "Graphics/GfxDevice.h"
#include "RenderGraph/RenderGraph.h"

namespace adria
{

	FidelityFXManager::FidelityFXManager(GfxDevice* gfx, uint32 width, uint32 height)
	{
		uint32 context_count = FFX_FSR2_CONTEXT_COUNT + FFX_FSR3UPSCALER_CONTEXT_COUNT + FFX_DOF_CONTEXT_COUNT + FFX_CAS_CONTEXT_COUNT + 2 * FFX_CACAO_CONTEXT_COUNT;

		ffx_interface = std::make_unique<FfxInterface>();
		uint64 const scratch_buffer_size = ffxGetScratchMemorySizeDX12(context_count);
		void* scratch_buffer = malloc(scratch_buffer_size);
		ADRIA_ASSERT(scratch_buffer);
		memset(scratch_buffer, 0, scratch_buffer_size);
		FfxErrorCode error_code = ffxGetInterfaceDX12(ffx_interface.get(), gfx->GetDevice(), scratch_buffer, scratch_buffer_size, context_count);
		ADRIA_ASSERT(error_code == FFX_OK);

		ffx_cacao = std::make_unique<FFXCACAOPass>(gfx, *ffx_interface, width, height);
		ffx_depth_of_field = std::make_unique<FFXDepthOfFieldPass>(gfx, *ffx_interface, width, height);
		ffx_cas = std::make_unique<FFXCASPass>(gfx, *ffx_interface, width, height);
		ffx_fsr2 = std::make_unique<FSR2Pass>(gfx, *ffx_interface, width, height);
		ffx_fsr3 = std::make_unique<FSR3Pass>(gfx, *ffx_interface, width, height);
	}


	FidelityFXManager::~FidelityFXManager()
	{
		ffx_fsr3.reset(nullptr);
		ffx_fsr2.reset(nullptr);
		ffx_cas.reset(nullptr);
		ffx_depth_of_field.reset(nullptr);
		ffx_cacao.reset(nullptr);
		if (ffx_interface->scratchBuffer)
		{
			free(ffx_interface->scratchBuffer);
			ffx_interface->scratchBuffer = nullptr;
		}
		ffx_interface.reset(nullptr);
	}

}
