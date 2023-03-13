#pragma once
#include <span>
#include "GfxFence.h"

namespace adria
{
	class GfxDevice;

	enum class GfxCommandQueueType : uint8
	{
		Graphics,
		Compute,
		Copy
	};

	class GfxCommandQueue
	{
		static constexpr D3D12_COMMAND_LIST_TYPE GetCommandListType(GfxCommandQueueType type);
	public:
		GfxCommandQueue() = default;
		~GfxCommandQueue() = default;

		bool Create(GfxDevice* gfx, GfxCommandQueueType type, char const* name = "");
		
		void ExecuteCommandLists(std::span<ID3D12CommandList*> cmd_lists);

		void Signal(GfxFence& fence, uint64 fence_value);
		void Wait(GfxFence& fence, uint64 fence_value);

		uint64 GetTimestampFrequency() const { return timestamp_frequency; }
		GfxCommandQueueType GetType() const { return type; }

		operator ID3D12CommandQueue* () const { return command_queue.Get(); }
	private:
		ArcPtr<ID3D12CommandQueue> command_queue;
		GfxFence queue_fence;
		uint64 timestamp_frequency;
		GfxCommandQueueType type;
	};
}