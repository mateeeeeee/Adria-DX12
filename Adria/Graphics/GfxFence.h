#pragma once
#include <d3d12.h>

namespace adria
{
	class GfxDevice;
	class GfxCommandQueue;

	class GfxFence
	{
	public:
		GfxFence();
		~GfxFence();

		bool Create(GfxDevice* gfx, char const* name);

		void Wait(uint64 value);
		void Signal(uint64 value);

		bool IsCompleted(uint64 value);
		uint64 GetCompletedValue() const;

		operator ID3D12Fence* () const { return fence.Get(); }
	private:
		Ref<ID3D12Fence> fence = nullptr;
		HANDLE event = nullptr;
	};
}