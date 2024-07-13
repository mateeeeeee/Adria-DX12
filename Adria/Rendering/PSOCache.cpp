#include "PSOCache.h"
#include "ShaderManager.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxPipelineState.h"
#include "Graphics/GfxStateObject.h"
#include "Graphics/GfxStates.h"
#include "Graphics/GfxShaderCompiler.h"
#include "Graphics/GfxReflection.h"
#include "Logging/Logger.h"
#include "Utilities/Timer.h"

namespace adria
{
	namespace
	{
		GfxDevice* gfx;
		std::unordered_map<GfxPipelineStateID, std::unique_ptr<ComputePipelineState>>		compute_pso_map;

		void CreateAllPSOs()
		{
			ComputePipelineStateDesc compute_pso_desc{};
			{
				compute_pso_desc.CS = CS_Picking;
				compute_pso_map[GfxPipelineStateID::Picking] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_Ssr;
				compute_pso_map[GfxPipelineStateID::SSR] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_ExponentialHeightFog;
				compute_pso_map[GfxPipelineStateID::ExponentialHeightFog] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_MotionVectors;
				compute_pso_map[GfxPipelineStateID::MotionVectors] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_MotionBlur;
				compute_pso_map[GfxPipelineStateID::MotionBlur] = gfx->CreateComputePipelineState(compute_pso_desc);

				compute_pso_desc.CS = CS_GodRays;
				compute_pso_map[GfxPipelineStateID::GodRays] = gfx->CreateComputePipelineState(compute_pso_desc);
				
				compute_pso_desc.CS = CS_FilmEffects;
				compute_pso_map[GfxPipelineStateID::FilmEffects] = gfx->CreateComputePipelineState(compute_pso_desc);
			}
		}
	}

	void PSOCache::Initialize(GfxDevice* _gfx)
	{
		gfx = _gfx;
		CreateAllPSOs();
	}

	void PSOCache::Destroy()
	{
		compute_pso_map.clear();
	}

	GfxPipelineState* PSOCache::Get(GfxPipelineStateID ps)
	{
		return compute_pso_map[ps].get();
	}
}

