#pragma once
#include <span>
#include "GfxFence.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;

	enum class GfxCommandListType : uint8;

	class GfxCommandQueue
	{
	public:
		GfxCommandQueue() = default;
		~GfxCommandQueue() = default;

		bool Create(GfxDevice* gfx, GfxCommandListType type, char const* name = "");
		
		void ExecuteCommandLists(std::span<GfxCommandList*> cmd_lists);
		void ExecuteCommandLists(std::span<ID3D12CommandList*> cmd_lists);

		void Signal(GfxFence& fence, uint64 fence_value);
		void Wait(GfxFence& fence, uint64 fence_value);

		uint64 GetTimestampFrequency() const { return timestamp_frequency; }
		GfxCommandListType GetType() const { return type; }

		operator ID3D12CommandQueue* () const { return command_queue.Get(); }
	private:
		ArcPtr<ID3D12CommandQueue> command_queue;
		uint64 timestamp_frequency;
		GfxCommandListType type;
	};
}