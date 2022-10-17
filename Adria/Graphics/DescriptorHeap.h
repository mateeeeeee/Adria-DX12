#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <mutex>
#include <vector>
#include "../Core/Macros.h"

namespace adria
{

	struct DescriptorHandle
	{
		friend class DescriptorHeap;

	public:
		DescriptorHandle() = default;
		DescriptorHandle(DescriptorHandle const&) = default;
		DescriptorHandle(DescriptorHandle&&) noexcept = default;

		DescriptorHandle& operator=(DescriptorHandle const&) = default;
		DescriptorHandle& operator=(DescriptorHandle&&) noexcept = default;

		operator D3D12_CPU_DESCRIPTOR_HANDLE const() const { return cpu_pointer; }
		operator D3D12_GPU_DESCRIPTOR_HANDLE const() const { return gpu_pointer; }

		bool IsShaderVisible() const { return gpu_pointer.ptr != NULL; }
		D3D12_CPU_DESCRIPTOR_HANDLE const* GetCPUAddress() const { return &cpu_pointer; }
		SIZE_T GetHeapOffset() const { return offset_in_heap; }
	private:
		D3D12_CPU_DESCRIPTOR_HANDLE cpu_pointer = { NULL };
		D3D12_GPU_DESCRIPTOR_HANDLE gpu_pointer = { NULL };
		SIZE_T offset_in_heap = -1;

	private:
		explicit DescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu = { NULL }, SIZE_T offset = -1) 
			: cpu_pointer(cpu), gpu_pointer(gpu), offset_in_heap(offset)
		{}
	};

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

		DescriptorHandle GetFirstHandle() const;
		DescriptorHandle GetHandle(size_t index) const;

		size_t Count() const;
		D3D12_DESCRIPTOR_HEAP_FLAGS Flags() const;
		D3D12_DESCRIPTOR_HEAP_TYPE  Type() const;
		size_t Increment() const;
		ID3D12DescriptorHeap* Heap() const;

	protected:
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>	heap;
		D3D12_DESCRIPTOR_HEAP_DESC                      desc;
		D3D12_CPU_DESCRIPTOR_HANDLE                     hCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE                     hGPU;
		UINT	                                        descriptor_handle_size;
	private:

		void CreateHelper(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_DESC const& _desc);
	};

}