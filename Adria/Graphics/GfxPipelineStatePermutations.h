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
	struct IsGraphicsPipelineStateUtil
	{
		static constexpr bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::Graphics;
	};
	template<typename PSO>
	constexpr bool IsGraphicsPipelineState = IsGraphicsPipelineStateUtil<PSO>::value;

	template<typename PSO>
	struct IsComputePipelineStateUtil
	{
		static constexpr bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::Compute;
	};
	template<typename PSO>
	constexpr bool IsComputePipelineState = IsComputePipelineStateUtil<PSO>::value;

	template<typename PSO>
	struct IsMeshShaderPipelineStateUtil
	{
		static constexpr bool value = PSOTraits<PSO>::PipelineStateType == GfxPipelineStateType::MeshShader;
	};
	template<typename PSO>
	constexpr bool IsMeshShaderPipelineState = IsMeshShaderPipelineStateUtil<PSO>::value;

	template<typename PSO, uint32 N>
	class GfxPipelineStatePermutations
	{
		using PSODesc = PSOTraits<PSO>::PSODescType;
		static constexpr GfxPipelineStateType pso_type = PSOTraits<PSO>::PipelineStateType;

	public:
		GfxPipelineStatePermutations() = default;
		~GfxPipelineStatePermutations() = default;

		void Initialize(PSODesc const& desc)
		{
			for (auto& pso_desc : pso_descs) pso_desc = desc;
		}
		void Destroy()
		{
			for (uint32 i = 0; i < N; ++i)
			{
				if(pso_permutations[i]) pso_permutations[i].reset(nullptr);
			}
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

		template<uint32 P> requires !IsComputePipelineState<PSO>
		void SetCullMode(GfxCullMode cull_mode)
		{
			static_assert(P < N);
			PSODesc& desc = pso_descs[P];
			desc.rasterizer_state.cull_mode = cull_mode;
		}

		template<uint32 P> requires !IsComputePipelineState<PSO>
		void SetFillMode(GfxFillMode fill_mode)
		{
			static_assert(P < N);
			PSODesc& desc = pso_descs[P];
			desc.rasterizer_state.fill_mode = fill_mode;
		}

		template<uint32 P> requires !IsComputePipelineState<PSO>
		void SetTopologyType(GfxPrimitiveTopologyType topology_type)
		{
			static_assert(P < N);
			PSODesc& desc = pso_descs[P];
			desc.topology_type = topology_type;
		}

		template<uint32 P, typename F> requires std::is_invocable_v<F, PSODesc&>
		void ModifyDesc(F&& f)
		{
			static_assert(P < N);
			PSODesc& desc = pso_descs[P];
			f(desc);
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

	template<uint32 N> using GfxGraphicsPipelineStatePermutations		= GfxPipelineStatePermutations<GfxGraphicsPipelineState, N>;
	template<uint32 N> using GfxComputePipelineStatePermutations		= GfxPipelineStatePermutations<GfxComputePipelineState, N>;
	template<uint32 N> using GfxMeshShaderPipelineStatePermutations		= GfxPipelineStatePermutations<GfxMeshShaderPipelineState, N>;
}