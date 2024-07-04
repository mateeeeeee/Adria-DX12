#pragma once
#include <d3d12.h>

namespace adria
{
	class GfxDescriptor
	{
		friend class GfxDescriptorAllocatorBase;

	public:
		GfxDescriptor() {}
		ADRIA_DEFAULT_COPYABLE_MOVABLE(GfxDescriptor)

		operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return cpu; }
		operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return gpu; }

		uint32 GetIndex() const { return index; }
		void Increment(uint32 increment, uint32 multiply = 1)
		{
			cpu.ptr += increment * multiply;
			if(gpu.ptr != NULL) gpu.ptr += increment * multiply;
			index += multiply;
		}

		bool operator==(GfxDescriptor const& other) const
		{
			return cpu.ptr == other.cpu.ptr && index == other.index;
		}

		bool IsValid() const
		{
			return cpu.ptr != NULL;
		}

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = { NULL };
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = { NULL };
		uint32 index = -1;
	};
}