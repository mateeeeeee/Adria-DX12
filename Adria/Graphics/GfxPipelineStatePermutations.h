#pragma once
#include "GfxPipelineState.h"
#include "GfxShaderEnums.h"
#include "Utilities/HashUtil.h"

namespace adria
{
	template<typename PSO>
	struct PSOTraits;

	struct GfxGraphicsPipelineStateDescHash
	{
		ADRIA_NODISCARD Uint64 operator()(GfxGraphicsPipelineStateDesc const& desc) const
		{
			HashState state;
			state.Combine(crc64(reinterpret_cast<Char const*>(&desc), sizeof(GfxGraphicsPipelineStateDesc)));
			state.Combine(desc.VS.GetHash());
			state.Combine(desc.PS.GetHash());
			state.Combine(desc.DS.GetHash());
			state.Combine(desc.HS.GetHash());
			state.Combine(desc.GS.GetHash());
			return state;
		}
	};
	struct GfxComputePipelineStateDescHash
	{
		ADRIA_NODISCARD Uint64 operator()(GfxComputePipelineStateDesc const& desc) const
		{
			HashState state;
			state.Combine(crc64(reinterpret_cast<Char const*>(&desc), sizeof(GfxComputePipelineStateDesc)));
			state.Combine(desc.CS.GetHash());
			return state;
		}
	};
	struct GfxMeshShaderPipelineStateDescHash
	{
		ADRIA_NODISCARD Uint64 operator()(GfxMeshShaderPipelineStateDesc const& desc) const
		{
			HashState state;
			state.Combine(crc64(reinterpret_cast<Char const*>(&desc), sizeof(GfxMeshShaderPipelineStateDesc)));
			state.Combine(desc.AS.GetHash());
			state.Combine(desc.MS.GetHash());
			state.Combine(desc.PS.GetHash());
			return state;
		}
	};

	template<>
	struct PSOTraits<GfxGraphicsPipelineState>
	{
		static constexpr GfxPipelineStateType PipelineStateType = GfxPipelineStateType::Graphics;
		using PSODescType = GfxGraphicsPipelineStateDesc;
		using PSODescHasher = GfxGraphicsPipelineStateDescHash;
	};
	template<>
	struct PSOTraits<GfxComputePipelineState>
	{
		static constexpr GfxPipelineStateType PipelineStateType = GfxPipelineStateType::Compute;
		using PSODescType = GfxComputePipelineStateDesc;
		using PSODescHasher = GfxComputePipelineStateDescHash;
	};
	template<>
	struct PSOTraits<GfxMeshShaderPipelineState>
	{
		static constexpr GfxPipelineStateType PipelineStateType = GfxPipelineStateType::MeshShader;
		using PSODescType = GfxMeshShaderPipelineStateDesc;
		using PSODescHasher = GfxMeshShaderPipelineStateDescHash;
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
		using PSODesc = PSOTraits<PSO>::PSODescType;
		using PSODescHasher = PSOTraits<PSO>::PSODescHasher;
		//using PSOPermutationMap = std::unordered_map<PSODesc, std::unique_ptr<PSO>, PSODescHasher, PSODescComparator<PSODesc>>;
		using PSOPermutationMap = std::unordered_map<Uint64, std::unique_ptr<PSO>>;
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
			Uint64 pso_hash = PSODescHasher{}(current_pso_desc);
			if (!pso_permutations.contains(pso_hash))
			{
				pso_permutations[pso_hash] = std::make_unique<PSO>(gfx, current_pso_desc);
			}
			PSO* pso = pso_permutations[pso_hash].get();
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