#pragma once
#include <d3d12.h>
#include <wrl.h>

namespace adria
{
	class CopyManager
	{
	public:

		CopyManager(ID3D12Device* device);

		~CopyManager();

		ID3D12GraphicsCommandList* CopyCmdList() const;

		void Wait();

	private:
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>          copy_queue = nullptr;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator>      copy_allocator = nullptr;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>   copy_cmd_list = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Fence>                 copy_fence = nullptr;
		HANDLE						                        copy_fence_event = nullptr;
		UINT64						                        copy_fence_value;
	};
}

