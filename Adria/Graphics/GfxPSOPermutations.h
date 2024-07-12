#pragma once
#include "GfxPipelineState.h"

namespace adria
{
	inline void GetGraphicsGfxShaderKeys(GraphicsPipelineStateDesc& desc, std::vector<GfxShaderKey*>& keys)
	{
		keys.push_back(&desc.VS);
		keys.push_back(&desc.PS);
		keys.push_back(&desc.HS);
		keys.push_back(&desc.DS);
		keys.push_back(&desc.GS);
	}
	inline void GetComputeGfxShaderKeys(ComputePipelineStateDesc& desc, std::vector<GfxShaderKey*>& keys)
	{
		keys.push_back(&desc.CS);
	}
	inline void GetMeshShaderGfxShaderKeys(MeshShaderPipelineStateDesc& desc, std::vector<GfxShaderKey*>& keys)
	{
		keys.push_back(&desc.AS);
		keys.push_back(&desc.MS);
		keys.push_back(&desc.PS);
	}

	template<typename PSO>
	struct PSOTraits;

	template<>
	struct PSOTraits<GraphicsPipelineState>
	{
		static constexpr GfxPipelineStateType type = GfxPipelineStateType::Graphics;
		using PSODesc = GraphicsPipelineStateDesc;
		static constexpr void(*GetGfxShaderKeys)(PSODesc&, std::vector<GfxShaderKey*>&) = GetGraphicsGfxShaderKeys;
	};
	template<>
	struct PSOTraits<ComputePipelineState>
	{
		static constexpr GfxPipelineStateType type = GfxPipelineStateType::Compute;
		using PSODesc = ComputePipelineStateDesc;
		static constexpr void(*GetGfxShaderKeys)(PSODesc&, std::vector<GfxShaderKey*>&) = GetComputeGfxShaderKeys;
	};
	template<>
	struct PSOTraits<MeshShaderPipelineState>
	{
		static constexpr GfxPipelineStateType type = GfxPipelineStateType::MeshShader;
		using PSODesc = MeshShaderPipelineStateDesc;
		static constexpr void(*GetGfxShaderKeys)(PSODesc&, std::vector<GfxShaderKey*>&) = GetMeshShaderGfxShaderKeys;
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
			std::vector<GfxShaderKey*> keys;
			PSOTraits::GetGfxShaderKeys(pso_descs[P], keys);
			for (GfxShaderKey* key : keys) key->AddDefine(name, value);
		}

		template<uint32 P, typename F> requires std::is_invocable_v<F, PSODesc&>
		void ModifyDesc(F&& f)
		{
			static_assert(P < N);
			f(pso_descs[P]);
		}

		template<uint32 P> requires pso_type != GfxPipelineStateType::Compute
		void SetFillMode(GfxFillMode fill_mode)
		{
			static_assert(P < N);
			pso_descs[P].rasterizer_state.fill_mode = fill_mode;
		}

		template<uint32 P> requires pso_type != GfxPipelineStateType::Compute
		void SetCullingMode(GfxCullMode cull_mode)
		{
			static_assert(P < N);
			pso_descs[P].rasterizer_state.cull_mode = cull_mode;
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

	template<uint32 N> using GraphicsPSOPermutations	= GfxPSOPermutations<GraphicsPipelineState, N>;
	template<uint32 N> using ComputePSOPermutations		= GfxPSOPermutations<ComputePipelineState, N>;
	template<uint32 N> using MeshShaderPSOPermutations	= GfxPSOPermutations<MeshShaderPipelineState, N>;
	template<uint32 N> using GraphicsPSOPermutationsPtr		= std::unique_ptr<GraphicsPSOPermutations<N>>;
	template<uint32 N> using ComputePSOPermutationsPtr		= std::unique_ptr<ComputePSOPermutations<N>>;
	template<uint32 N> using MeshShaderPSOPermutationsPtr	= std::unique_ptr<MeshShaderPSOPermutations<N>>;
}