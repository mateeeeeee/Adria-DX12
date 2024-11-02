#pragma once
#include <span>
#include "GfxFence.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class GfxCommandListPool;

	enum class GfxCommandListType : Uint8;

	class GfxCommandQueue
	{
	public:
		GfxCommandQueue() = default;
		~GfxCommandQueue() = default;

		bool Create(GfxDevice* gfx, GfxCommandListType type, char const* name = "");
		
		void ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists);
		void ExecuteCommandListPool(GfxCommandListPool& cmd_list_pool);

		void Signal(GfxFence& fence, Uint64 fence_value);
		void Wait(GfxFence& fence, Uint64 fence_value);

		Uint64 GetTimestampFrequency() const { return timestamp_frequency; }
		GfxCommandListType GetType() const { return type; }

		operator ID3D12CommandQueue* () const { return command_queue.Get(); }
	private:
		Ref<ID3D12CommandQueue> command_queue;
		Uint64 timestamp_frequency;
		GfxCommandListType type;
	};
}