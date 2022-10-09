#include "DescriptorHeap.h"



namespace adria
{
	DescriptorHeap::DescriptorHeap(ID3D12DescriptorHeap* pExistingHeap)
		: heap{ pExistingHeap }
	{
		hCPU = pExistingHeap->GetCPUDescriptorHandleForHeapStart();
		hGPU = pExistingHeap->GetGPUDescriptorHandleForHeapStart();
		desc = pExistingHeap->GetDesc();

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		pExistingHeap->GetDevice(IID_PPV_ARGS(device.GetAddressOf()));

		descriptor_handle_size = device->GetDescriptorHandleIncrementSize(desc.Type);
	}

	DescriptorHeap::DescriptorHeap(
		ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_DESC const& _desc)
		: desc{},
		hCPU{},
		hGPU{},
		descriptor_handle_size(0)
	{
		CreateHelper(device, _desc);
	}

	DescriptorHeap::DescriptorHeap(
		ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE type,
		D3D12_DESCRIPTOR_HEAP_FLAGS flags,
		size_t count)
		: desc{},
		hCPU{},
		hGPU{},
		descriptor_handle_size(0)
	{
		ADRIA_ASSERT(count <= UINT32_MAX && "Too many descriptors");
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Flags = flags;
		desc.NumDescriptors = static_cast<UINT>(count);
		desc.Type = type;
		CreateHelper(device, desc);
	}

	DescriptorHeap::DescriptorHeap(
		ID3D12Device* device,
		size_t count) :
		DescriptorHeap(device,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, count) {}

	DescriptorHandle DescriptorHeap::GetFirstHandle() const
	{
		ADRIA_ASSERT(heap != nullptr);
		return DescriptorHandle(hCPU, hGPU);
	}
	DescriptorHandle DescriptorHeap::GetHandle(size_t index) const
	{
		ADRIA_ASSERT(heap != nullptr);
		ADRIA_ASSERT(index < desc.NumDescriptors);

		D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
		cpu_handle.ptr = static_cast<SIZE_T>(hCPU.ptr + UINT64(index) * UINT64(descriptor_handle_size));

		D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{.ptr = NULL};
		if(hGPU.ptr != NULL) gpu_handle.ptr = hGPU.ptr + UINT64(index) * UINT64(descriptor_handle_size);

		DescriptorHandle handle{ cpu_handle, gpu_handle, index };
		return handle;
	}

	size_t DescriptorHeap::Count() const { return desc.NumDescriptors; }
	D3D12_DESCRIPTOR_HEAP_FLAGS DescriptorHeap::Flags() const { return desc.Flags; }
	D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeap::Type() const { return desc.Type; }
	size_t DescriptorHeap::Increment() const { return descriptor_handle_size; }
	ID3D12DescriptorHeap* DescriptorHeap::Heap() const { return heap.Get(); }
	void DescriptorHeap::CreateHelper(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_DESC const& _desc)
	{
		desc = _desc;
		descriptor_handle_size = pDevice->GetDescriptorHandleIncrementSize(desc.Type);

		if (desc.NumDescriptors == 0)
		{
			heap.Reset();
			hCPU.ptr = 0;
			hGPU.ptr = 0;
		}
		else
		{
			BREAK_IF_FAILED(pDevice->CreateDescriptorHeap(
				&desc,
				IID_PPV_ARGS(heap.ReleaseAndGetAddressOf())));
			heap->SetName(L"DescriptorHeap");
			hCPU = heap->GetCPUDescriptorHandleForHeapStart();
			if (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
				hGPU = heap->GetGPUDescriptorHandleForHeapStart();

		}
	}
}