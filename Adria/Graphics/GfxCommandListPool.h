#pragma once
#include <vector>
#include <memory>
#include "GfxCommandList.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	enum class GfxCommandListType : uint8;

	class GfxCommandListPool
	{
	public:
		ADRIA_NONCOPYABLE(GfxCommandListPool);
		ADRIA_DEFAULT_MOVABLE(GfxCommandListPool);
		~GfxCommandListPool() = default;

		GfxCommandList* GetMainCmdList() const;
		GfxCommandList* GetLatestCmdList() const;

		GfxCommandList* AllocateCmdList();
		void FreeCmdList(GfxCommandList* _cmd_list);

	protected:
		GfxCommandListPool(GfxDevice* gfx, GfxCommandListType type);

	private:
		GfxDevice* gfx;
		GfxCommandListType const type;
		std::vector<std::unique_ptr<GfxCommandList>> cmd_lists;
	};

	class GfxGraphicsCommandListPool : public GfxCommandListPool
	{
	public:
		explicit GfxGraphicsCommandListPool(GfxDevice* gfx);
	};

	class GfxComputeCommandListPool : public GfxCommandListPool
	{
	public:
		explicit GfxComputeCommandListPool(GfxDevice* gfx);
	};

	class GfxCopyCommandListPool : public GfxCommandListPool
	{
	public:
		explicit GfxCopyCommandListPool(GfxDevice* gfx);
	};

}