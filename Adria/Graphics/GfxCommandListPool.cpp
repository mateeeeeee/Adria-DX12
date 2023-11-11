#include "GfxCommandListPool.h"
#include "GfxCommandList.h"

namespace adria
{

	GfxCommandListPool::GfxCommandListPool(GfxDevice* gfx, GfxCommandListType type) : gfx(gfx), type(type)
	{
		cmd_lists.push_back(std::make_unique<GfxCommandList>(gfx, type));
	}

	GfxCommandList* GfxCommandListPool::GetMainCmdList() const
	{
		return cmd_lists.front().get();
	}
	GfxCommandList* GfxCommandListPool::GetLatestCmdList() const
	{
		return cmd_lists.back().get();
	}

	GfxCommandList* GfxCommandListPool::AllocateCmdList()
	{
		cmd_lists.push_back(std::make_unique<GfxCommandList>(gfx, type));
		return cmd_lists.back().get();
	}
	void GfxCommandListPool::FreeCmdList(GfxCommandList* _cmd_list)
	{
		for (auto& cmd_list : cmd_lists)
		{
			if (cmd_list.get() == _cmd_list)
			{
				cmd_list.swap(cmd_lists.back());
				break;
			}
		}
		cmd_lists.pop_back();
	}

	GfxGraphicsCommandListPool::GfxGraphicsCommandListPool(GfxDevice* gfx) : GfxCommandListPool(gfx, GfxCommandListType::Graphics)
	{

	}
	GfxComputeCommandListPool::GfxComputeCommandListPool(GfxDevice* gfx) : GfxCommandListPool(gfx, GfxCommandListType::Compute)
	{

	}
	GfxCopyCommandListPool::GfxCopyCommandListPool(GfxDevice* gfx) : GfxCommandListPool(gfx, GfxCommandListType::Copy)
	{

	}

}

