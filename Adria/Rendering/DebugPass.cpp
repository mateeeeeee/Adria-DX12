#include "DebugPass.h"
#include "ShaderStructs.h"
#include "Components.h"
#include "Enums.h"
#include "BlackboardData.h"
#include "PSOCache.h" 

#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Graphics/GfxRingDescriptorAllocator.h"
#include "RenderGraph/RenderGraph.h"
#include "entt/entity/registry.hpp"

using namespace DirectX;

namespace adria
{

	DebugPass::DebugPass(entt::registry& reg, uint32 w, uint32 h)
		: reg(reg), width(w), height(h)
	{}

	void DebugPass::AddPass(RenderGraph& rg)
	{
		FrameBlackboardData const& global_data = rg.GetBlackboard().GetChecked<FrameBlackboardData>();
		rg.AddPass<void>("AABB Pass",
			[=](RenderGraphBuilder& builder)
			{
				builder.WriteRenderTarget(RG_RES_NAME(HDR_RenderTarget), RGLoadStoreAccessOp::Preserve_Preserve);
				builder.SetViewport(width, height);
			},
			[=](RenderGraphContext& context, GfxCommandList* cmd_list)
			{
				
			}, RGPassType::Graphics, RGPassFlags::None);
	}

	void DebugPass::OnSceneInitialized(GfxDevice* gfx)
	{
		CreateIndexBuffer(gfx);
	}

	void DebugPass::OnResize(uint32 w, uint32 h)
	{
		width = w, height = h;
	}

	void DebugPass::CreateIndexBuffer(GfxDevice* gfx)
	{
		const uint16 aabb_indices[] = {
			0, 1,
			1, 2,
			2, 3,
			3, 0,

			0, 4,
			1, 5,
			2, 6,
			3, 7,

			4, 5,
			5, 6,
			6, 7,
			7, 4
		};
		aabb_ib = std::make_unique<GfxBuffer>(gfx, IndexBufferDesc(ARRAYSIZE(aabb_indices), true), aabb_indices);
	}
}

