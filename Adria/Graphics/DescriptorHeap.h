#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <mutex>
#include <vector>
#include "../Core/Macros.h"

//https://github.com/microsoft/DirectXTK12/blob/master/Inc/DescriptorHeap.h

namespace adria
{

	class DescriptorHeap
	{
	public:
		DescriptorHeap(ID3D12DescriptorHeap* pExistingHeap);

		DescriptorHeap(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_DESC const& _desc);

		DescriptorHeap(
			ID3D12Device* device,
			D3D12_DESCRIPTOR_HEAP_TYPE type,
			D3D12_DESCRIPTOR_HEAP_FLAGS flags,
			size_t count);

		DescriptorHeap(
			ID3D12Device* device,
			size_t count);

		DescriptorHeap(DescriptorHeap const&) = delete;
		DescriptorHeap(DescriptorHeap&&) = default;

		DescriptorHeap& operator=(DescriptorHeap const&) = delete;
		DescriptorHeap& operator=(DescriptorHeap&&) = default;

		~DescriptorHeap() = default;

		D3D12_GPU_DESCRIPTOR_HANDLE GetFirstGpuHandle() const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetFirstCpuHandle() const;

		D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t index) const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t index) const;

		size_t Count() const;
		D3D12_DESCRIPTOR_HEAP_FLAGS Flags() const;
		D3D12_DESCRIPTOR_HEAP_TYPE  Type() const;
		size_t Increment() const;
		ID3D12DescriptorHeap* Heap() const;

	private:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	heap;
		D3D12_DESCRIPTOR_HEAP_DESC                      desc;
		D3D12_CPU_DESCRIPTOR_HANDLE                     hCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE                     hGPU;
		UINT	                                        descriptor_handle_size;

	private:

		void CreateHelper(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_DESC const& _desc);
	};

}