#pragma once
#include <d3d12.h>
#include "../Utilities/AutoRefCountPtr.h"
#include "../Core/Definitions.h"


namespace adria
{
	class GfxDevice;
	class GfxCommandQueue;

	enum class GfxCommandListType : uint8
	{
		Graphics,
		Compute,
		Copy
	};

	inline constexpr D3D12_COMMAND_LIST_TYPE GetCommandListType(GfxCommandListType type)
	{
		switch (type)
		{
		case GfxCommandListType::Graphics:
			return D3D12_COMMAND_LIST_TYPE_DIRECT;
		case GfxCommandListType::Compute:
			return D3D12_COMMAND_LIST_TYPE_COMPUTE;
		case GfxCommandListType::Copy:
			return D3D12_COMMAND_LIST_TYPE_COPY;
		}
		return D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

	class GfxCommandList
	{
	public:
		explicit GfxCommandList(GfxDevice* gfx, GfxCommandListType type = GfxCommandListType::Graphics, char const* name = "");

		ID3D12GraphicsCommandList4* GetNative() const { return cmd_list.Get(); }
		GfxCommandQueue& GetQueue() const { return cmd_queue; }

		void ResetAllocator();
	private:
		GfxDevice* gfx = nullptr;
		GfxCommandQueue& cmd_queue;
		ArcPtr<ID3D12GraphicsCommandList4> cmd_list;
		ArcPtr<ID3D12CommandAllocator> cmd_allocator;

		uint32 command_count = 0;
	};
}