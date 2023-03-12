#pragma once
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
	public:
		GfxCommandQueue() = default;

		bool Create(GfxDevice* gfx, GfxCommandQueueType type);
		void Signal(GfxFence& fence);

		uint64 GetTimestampFrequency() const { return timestamp_frequency; }
		GfxCommandQueueType GetType() const { return type; }
	private:
		ArcPtr<ID3D12CommandQueue> command_queue;
		GfxFence queue_fence;
		uint64 timestamp_frequency;
		GfxCommandQueueType type;
	};
}