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

		Bool Create(GfxDevice* gfx, Char const* name);

		void Wait(Uint64 value);
		void Signal(Uint64 value);

		Bool IsCompleted(Uint64 value);
		Uint64 GetCompletedValue() const;

		operator ID3D12Fence* () const { return fence.Get(); }

	private:
		Ref<ID3D12Fence> fence = nullptr;
		HANDLE event = nullptr;
	};
}