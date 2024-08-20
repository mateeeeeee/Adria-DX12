#pragma once

namespace adria
{
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;
	class GfxMeshShaderPipelineState;

	template<typename PSO>
	class GfxPipelineStatePermutations;

	using GfxGraphicsPipelineStatePermutations		= GfxPipelineStatePermutations<GfxGraphicsPipelineState>;
	using GfxComputePipelineStatePermutations		= GfxPipelineStatePermutations<GfxComputePipelineState>;
	using GfxMeshShaderPipelineStatePermutations	= GfxPipelineStatePermutations<GfxMeshShaderPipelineState>;
}

