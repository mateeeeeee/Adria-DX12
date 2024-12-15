#pragma once
#include "GfxPipelineState.h"
#include "GfxShaderEnums.h"
#include "Utilities/HashUtil.h"

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
	struct IsGraphicsPipelineStateImpl
	{
		static constexpr Bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::Graphics;
	};
	template<typename PSO>
	constexpr Bool IsGraphicsPipelineState = IsGraphicsPipelineStateImpl<PSO>::value;

	template<typename PSO>
	struct IsComputePipelineStateImpl
	{
		static constexpr Bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::Compute;
	};
	template<typename PSO>
	constexpr Bool IsComputePipelineState = IsComputePipelineStateImpl<PSO>::value;

	template<typename PSO>
	struct IsMeshShaderPipelineStateImpl
	{
		static constexpr Bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::MeshShader;
	};
	template<typename PSO>
	constexpr Bool IsMeshShaderPipelineState = IsMeshShaderPipelineStateImpl<PSO>::value;

	template<typename PSO>
	class GfxPipelineStatePermutations
	{
		template <typename PSODesc>
		struct PSODescHash
		{
			ADRIA_NODISCARD Uint64 operator()(PSODesc const& desc) const
			{
				return crc64(reinterpret_cast<Char const*>(&desc), sizeof(PSODesc));
			}
		};
		template <typename PSODesc>
		struct PSODescComparator
		{
			ADRIA_NODISCARD Bool operator()(PSODesc const& lhs, PSODesc const& rhs) const
			{
				return true; //todo
			}
		};
		using PSODesc = PSOTraits<PSO>::PSODescType;
		using PSOPermutationMap = std::unordered_map<PSODesc, std::unique_ptr<PSO>, PSODescHash<PSODesc>, PSODescComparator<PSODesc>>;
		static constexpr GfxPipelineStateType PSOType = PSOTraits<PSO>::PipelineStateType;

	public:
		GfxPipelineStatePermutations(GfxDevice* gfx, PSODesc const& desc)
			: gfx(gfx), base_pso_desc(desc), current_pso_desc(desc)
		{
		}
		~GfxPipelineStatePermutations() = default;
		ADRIA_NONCOPYABLE(GfxPipelineStatePermutations)

		void AddDefine(Char const* name, Char const* value)
		{
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				current_pso_desc.VS.AddDefine(name, value);
				current_pso_desc.PS.AddDefine(name, value);
				current_pso_desc.DS.AddDefine(name, value);
				current_pso_desc.HS.AddDefine(name, value);
				current_pso_desc.GS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				current_pso_desc.CS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				current_pso_desc.MS.AddDefine(name, value);
				current_pso_desc.AS.AddDefine(name, value);
				current_pso_desc.PS.AddDefine(name, value);
			}
		}
		void AddDefine(Char const* name)
		{
			AddDefine(name, "");
		}
		template<GfxShaderStage stage>
		void AddDefine(Char const* name, Char const* value)
		{
			if constexpr (PSOType == GfxPipelineStateType::Graphics)
			{
				if (stage == GfxShaderStage::VS) current_pso_desc.VS.AddDefine(name, value);
				if (stage == GfxShaderStage::PS) current_pso_desc.PS.AddDefine(name, value);
				if (stage == GfxShaderStage::DS) current_pso_desc.DS.AddDefine(name, value);
				if (stage == GfxShaderStage::HS) current_pso_desc.HS.AddDefine(name, value);
				if (stage == GfxShaderStage::GS) current_pso_desc.GS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::Compute)
			{
				if (stage == GfxShaderStage::CS) current_pso_desc.CS.AddDefine(name, value);
			}
			else if constexpr (PSOType == GfxPipelineStateType::MeshShader)
			{
				if (stage == GfxShaderStage::MS) current_pso_desc.MS.AddDefine(name, value);
				if (stage == GfxShaderStage::AS) current_pso_desc.AS.AddDefine(name, value);
				if (stage == GfxShaderStage::PS) current_pso_desc.PS.AddDefine(name, value);
			}
		}
		template<GfxShaderStage stage>
		void AddDefine(Char const* name)
		{
			AddDefine<stage>(name, "");
		}

		void SetCullMode(GfxCullMode cull_mode) requires !IsComputePipelineState<PSO>
		{
			current_pso_desc.rasterizer_state.cull_mode = cull_mode;
		}
		void SetFillMode(GfxFillMode fill_mode) requires !IsComputePipelineState<PSO>
		{
			current_pso_desc.rasterizer_state.fill_mode = fill_mode;
		}
		void SetTopologyType(GfxPrimitiveTopologyType topology_type) requires !IsComputePipelineState<PSO>
		{
			current_pso_desc.topology_type = topology_type;
		}

		template<typename F> requires std::is_invocable_v<F, PSODesc&>
		void ModifyDesc(F&& f)
		{
			f(current_pso_desc);
		}

		PSO* Get() const
		{
			if (!pso_permutations.contains(current_pso_desc))
			{
				pso_permutations[current_pso_desc] = std::make_unique<PSO>(gfx, current_pso_desc);
			}
			PSO* pso = pso_permutations[current_pso_desc].get();
			current_pso_desc = base_pso_desc;
			return pso;
		}

	private:
		GfxDevice* gfx;
		PSODesc const base_pso_desc;
		mutable PSOPermutationMap pso_permutations;
		mutable PSODesc current_pso_desc;
	};

	using GfxGraphicsPipelineStatePermutations	 = GfxPipelineStatePermutations<GfxGraphicsPipelineState>;
	using GfxComputePipelineStatePermutations	 = GfxPipelineStatePermutations<GfxComputePipelineState>;
	using GfxMeshShaderPipelineStatePermutations = GfxPipelineStatePermutations<GfxMeshShaderPipelineState>;
}