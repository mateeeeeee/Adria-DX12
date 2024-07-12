#pragma once
#include "GfxPipelineState.h"
#include "GfxShaderEnums.h"

namespace adria
{
	template<typename PSO>
	struct PSOTraits;

	template<>
	struct PSOTraits<GraphicsPipelineState>
	{
		static constexpr GfxPipelineStateType type = GfxPipelineStateType::Graphics;
		using PSODesc = GraphicsPipelineStateDesc;
	};
	template<>
	struct PSOTraits<ComputePipelineState>
	{
		static constexpr GfxPipelineStateType type = GfxPipelineStateType::Compute;
		using PSODesc = ComputePipelineStateDesc;
	};
	template<>
	struct PSOTraits<MeshShaderPipelineState>
	{
		static constexpr GfxPipelineStateType type = GfxPipelineStateType::MeshShader;
		using PSODesc = MeshShaderPipelineStateDesc;
	};

	template<typename PSO, uint32 N>
	class GfxPSOPermutations
	{
		using PSODesc = PSOTraits<PSO>::PSODesc;
		static constexpr GfxPipelineStateType pso_type = PSOTraits<PSO>::type;

	public:
		GfxPSOPermutations() = default;
		~GfxPSOPermutations() = default;

		void Init(PSODesc const& desc)
		{
			for (auto& pso_desc : pso_descs) pso_desc = desc;
		}

		template<uint32 P>
		void AddDefine(char const* name, char const* value)
		{
			static_assert(P < N);
			PSODesc& desc = pso_descs[P];
			if constexpr (pso_type == GfxPipelineStateType::Graphics)
			{
				desc.VS.AddDefine(name, value);
				desc.PS.AddDefine(name, value);
				desc.DS.AddDefine(name, value);
				desc.HS.AddDefine(name, value);
				desc.GS.AddDefine(name, value);
			}
			else if constexpr (pso_type == GfxPipelineStateType::Compute)
			{
				desc.CS.AddDefine(name, value);
			}
			else if constexpr (pso_type == GfxPipelineStateType::MeshShader)
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
			static_assert(P < N);
			PSODesc& desc = pso_descs[P];
			if constexpr (pso_type == GfxPipelineStateType::Graphics)
			{
				if(stage == GfxShaderStage::VS) desc.VS.AddDefine(name, value);
				if(stage == GfxShaderStage::PS) desc.PS.AddDefine(name, value);
				if(stage == GfxShaderStage::DS) desc.DS.AddDefine(name, value);
				if(stage == GfxShaderStage::HS) desc.HS.AddDefine(name, value);
				if(stage == GfxShaderStage::GS) desc.GS.AddDefine(name, value);
			}
			else if constexpr (pso_type == GfxPipelineStateType::Compute)
			{
				if (stage == GfxShaderStage::CS) desc.CS.AddDefine(name, value);
			}
			else if constexpr (pso_type == GfxPipelineStateType::MeshShader)
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

		template<uint32 P, typename F> requires std::is_invocable_v<F, PSODesc&>
		void ModifyDesc(F&& f)
		{
			static_assert(P < N);
			f(pso_descs[P]);
		}

		void Finalize(GfxDevice* gfx)
		{
			for (uint32 i = 0; i < N; ++i)
			{
				pso_permutations[i] = std::make_unique<PSO>(gfx, pso_descs[i]);
			}
		}

		template<uint32 P>
		PSO* Get() const
		{
			static_assert(P < N);
			return pso_permutations[P].get();
		}

		PSO* operator[](uint32 i) const
		{
			return pso_permutations[i].get();
		}

	private:
		std::unique_ptr<PSO> pso_permutations[N];
		PSODesc pso_descs[N];
	};
	#define PSOPermutationKeyDefine(p, i, name, ...) p.AddDefine<i>(ADRIA_STRINGIFY(name)__VA_OPT__(,) ADRIA_STRINGIFY(__VA_ARGS__))


	template<uint32 N> using GraphicsPSOPermutations	= GfxPSOPermutations<GraphicsPipelineState, N>;
	template<uint32 N> using ComputePSOPermutations		= GfxPSOPermutations<ComputePipelineState, N>;
	template<uint32 N> using MeshShaderPSOPermutations	= GfxPSOPermutations<MeshShaderPipelineState, N>;
	template<uint32 N> using GraphicsPSOPermutationsPtr		= std::unique_ptr<GraphicsPSOPermutations<N>>;
	template<uint32 N> using ComputePSOPermutationsPtr		= std::unique_ptr<ComputePSOPermutations<N>>;
	template<uint32 N> using MeshShaderPSOPermutationsPtr	= std::unique_ptr<MeshShaderPSOPermutations<N>>;
}