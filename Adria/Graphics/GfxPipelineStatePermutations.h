#pragma once
#include "GfxPipelineState.h"
#include "GfxShaderEnums.h"

namespace adria
{
	template<typename PSO>
	struct PSOTraits;

	template<>
	struct PSOTraits<GfxGraphicsPipelineState>
	{
		static constexpr GfxPipelineStateType PipelineStateType = GfxPipelineStateType::Graphics;
		using PSODescType = GfxGraphicsPipelineStateDesc;
	};
	template<>
	struct PSOTraits<GfxComputePipelineState>
	{
		static constexpr GfxPipelineStateType PipelineStateType = GfxPipelineStateType::Compute;
		using PSODescType = GfxComputePipelineStateDesc;
	};
	template<>
	struct PSOTraits<GfxMeshShaderPipelineState>
	{
		static constexpr GfxPipelineStateType PipelineStateType = GfxPipelineStateType::MeshShader;
		using PSODescType = GfxMeshShaderPipelineStateDesc;
	};

	template<typename PSO>
	struct IsGraphicsPipelineState
	{
		static constexpr bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::Graphics;
	};
	template<typename PSO>
	constexpr bool IsGraphicsPipelineStateV = IsGraphicsPipelineState<PSO>::value;

	template<typename PSO>
	struct IsComputePipelineState
	{
		static constexpr bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::Compute;
	};
	template<typename PSO>
	constexpr bool IsComputePipelineStateV = IsComputePipelineState<PSO>::value;

	template<typename PSO>
	struct IsMeshShaderPipelineState
	{
		static constexpr bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::MeshShader;
	};
	template<typename PSO>
	constexpr bool IsMeshShaderPipelineStateV = IsMeshShaderPipelineState<PSO>::value;

	template<typename PSO>
	class GfxPipelineStatePermutations
	{
		using PSODesc = PSOTraits<PSO>::PSODescType;
		static constexpr GfxPipelineStateType PSOType = PSOTraits<PSO>::PipelineStateType;

	public:
		GfxPipelineStatePermutations(uint32 size, PSODesc const& desc)
		{
			pso_permutations.resize(size);
			pso_descs.resize(size);
			for (auto& pso_desc : pso_descs) pso_desc = desc;
		}
		~GfxPipelineStatePermutations() = default;

		template<uint32 P>
		void AddDefine(char const* name, char const* value)
		{
			ADRIA_ASSERT(P < pso_descs.size());
			PSODesc& desc = pso_descs[P];
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				desc.VS.AddDefine(name, value);
				desc.PS.AddDefine(name, value);
				desc.DS.AddDefine(name, value);
				desc.HS.AddDefine(name, value);
				desc.GS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				desc.CS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				desc.MS.AddDefine(name, value);
				desc.AS.AddDefine(name, value);
				desc.PS.AddDefine(name, value);
			}
		}
		template<uint32 P>
		void AddDefine(char const* name)
		{
			AddDefine<P>(name, "");
		}

		template<GfxShaderStage stage, uint32 P>
		void AddDefine(char const* name, char const* value)
		{
			ADRIA_ASSERT(P < pso_descs.size());
			PSODesc& desc = pso_descs[P];
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				if (stage == GfxShaderStage::VS) desc.VS.AddDefine(name, value);
				if (stage == GfxShaderStage::PS) desc.PS.AddDefine(name, value);
				if (stage == GfxShaderStage::DS) desc.DS.AddDefine(name, value);
				if (stage == GfxShaderStage::HS) desc.HS.AddDefine(name, value);
				if (stage == GfxShaderStage::GS) desc.GS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				if (stage == GfxShaderStage::CS) desc.CS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				if (stage == GfxShaderStage::MS) desc.MS.AddDefine(name, value);
				if (stage == GfxShaderStage::AS) desc.AS.AddDefine(name, value);
				if (stage == GfxShaderStage::PS) desc.PS.AddDefine(name, value);
			}
		}
		template<GfxShaderStage stage, uint32 P>
		void AddDefine(char const* name)
		{
			AddDefine<stage, P>(name, "");
		}

		template<uint32 P> requires !IsComputePipelineStateV<PSO>
		void SetCullMode(GfxCullMode cull_mode)
		{
			ADRIA_ASSERT(P < pso_descs.size());
			PSODesc& desc = pso_descs[P];
			desc.rasterizer_state.cull_mode = cull_mode;
		}

		template<uint32 P> requires !IsComputePipelineStateV<PSO>
		void SetFillMode(GfxFillMode fill_mode)
		{
			ADRIA_ASSERT(P < pso_descs.size());
			PSODesc& desc = pso_descs[P];
			desc.rasterizer_state.fill_mode = fill_mode;
		}

		template<uint32 P> requires !IsComputePipelineStateV<PSO>
		void SetTopologyType(GfxPrimitiveTopologyType topology_type)
		{
			ADRIA_ASSERT(P < pso_descs.size());
			PSODesc& desc = pso_descs[P];
			desc.topology_type = topology_type;
		}

		template<uint32 P, typename F> requires std::is_invocable_v<F, PSODesc&>
		void ModifyDesc(F&& f)
		{
			ADRIA_ASSERT(P < pso_descs.size());
			PSODesc& desc = pso_descs[P];
			f(desc);
		}

		void Finalize(GfxDevice* gfx)
		{
			for (uint32 i = 0; i < pso_permutations.size(); ++i)
			{
				pso_permutations[i] = std::make_unique<PSO>(gfx, pso_descs[i]);
			}
			pso_descs.clear();
		}

		template<uint32 P>
		PSO* Get() const
		{
			ADRIA_ASSERT(P < pso_permutations.size());
			return pso_permutations[P].get();
		}
		PSO* Get(uint32 p) const
		{
			ADRIA_ASSERT(p < pso_permutations.size());
			return pso_permutations[p].get();
		}

	private:
		std::vector<std::unique_ptr<PSO>> pso_permutations;
		std::vector<PSODesc> pso_descs;
	};

	using GfxGraphicsPipelineStatePermutations	 = GfxPipelineStatePermutations<GfxGraphicsPipelineState>;
	using GfxComputePipelineStatePermutations	 = GfxPipelineStatePermutations<GfxComputePipelineState>;
	using GfxMeshShaderPipelineStatePermutations = GfxPipelineStatePermutations<GfxMeshShaderPipelineState>;
}