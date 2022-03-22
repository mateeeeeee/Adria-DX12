#pragma once
#include <memory>
#include <DirectXMath.h>
#include "../Graphics/ReadbackBuffer.h"
#include "../Graphics/StructuredBuffer.h"
#include "../Graphics/ShaderUtility.h"
#include "../Graphics/ResourceBarrierBatch.h"
#include "../Graphics/GraphicsCoreDX12.h" 
#include "../Logging/Logger.h"


namespace adria
{
	struct PickingData
	{
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT4 normal;
	};

	class Picker
	{
		friend class Renderer;
	private:
		Picker(GraphicsCoreDX12* gfx) : gfx(gfx), write_picking_buffer(gfx->GetDevice(), 1), 
			read_picking_buffer{ {gfx->GetDevice(), sizeof(PickingData)}, {gfx->GetDevice(), sizeof(PickingData)}, {gfx->GetDevice(), sizeof(PickingData)} }
		{
			ID3D12Device* device = gfx->GetDevice();
			ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/PickerCS.cso", picker_blob);
			BREAK_IF_FAILED(device->CreateRootSignature(0, picker_blob.GetPointer(), picker_blob.GetLength(),
				IID_PPV_ARGS(picker_rs.GetAddressOf())));

			D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
			pso_desc.pRootSignature = picker_rs.Get();
			pso_desc.CS = picker_blob;
			BREAK_IF_FAILED(device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(picker_pso.GetAddressOf())));
		}

		void CreateView(D3D12_CPU_DESCRIPTOR_HANDLE uav_handle)
		{
			write_picking_buffer.CreateUAV(uav_handle);
		}

		void Pick(ID3D12GraphicsCommandList4* cmd_list, 
			D3D12_CPU_DESCRIPTOR_HANDLE depth_handle, 
			D3D12_CPU_DESCRIPTOR_HANDLE normal_handle,
			D3D12_GPU_VIRTUAL_ADDRESS frame_cbuffer_gpu_address)
		{
			ID3D12Device* device = gfx->GetDevice();
			LinearDescriptorAllocator* descriptor_allocator = gfx->GetDescriptorAllocator();
			UINT backbuffer_index = gfx->BackbufferIndex();

			cmd_list->SetComputeRootSignature(picker_rs.Get());
			cmd_list->SetPipelineState(picker_pso.Get());

			cmd_list->SetComputeRootConstantBufferView(0, frame_cbuffer_gpu_address);
			D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[] = { depth_handle, normal_handle };
			uint32 src_range_sizes[] = { 1,1 };

			OffsetType descriptor_index = descriptor_allocator->AllocateRange(_countof(cpu_handles));
			D3D12_CPU_DESCRIPTOR_HANDLE dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			uint32 dst_range_sizes[] = { (uint32)_countof(cpu_handles) };
			device->CopyDescriptors(1, &dst_descriptor, dst_range_sizes, _countof(cpu_handles), cpu_handles, src_range_sizes,
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetGpuHandle(descriptor_index));

			descriptor_index = descriptor_allocator->Allocate();
			dst_descriptor = descriptor_allocator->GetCpuHandle(descriptor_index);
			device->CopyDescriptorsSimple(1, dst_descriptor, write_picking_buffer.UAV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			cmd_list->SetComputeRootDescriptorTable(2, descriptor_allocator->GetGpuHandle(descriptor_index));

			ResourceBarrierBatch barrier_batch{};
			barrier_batch.AddTransition(write_picking_buffer.Buffer(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
			barrier_batch.Submit(cmd_list);

			cmd_list->Dispatch(1, 1, 1);
			cmd_list->CopyResource(read_picking_buffer[backbuffer_index].Resource(), write_picking_buffer.Buffer());

			barrier_batch.ReverseTransitions();
			barrier_batch.Submit(cmd_list);
		}

		PickingData GetPickingData() const
		{
			UINT backbuffer_index = gfx->BackbufferIndex();

			PickingData const* data = read_picking_buffer[backbuffer_index].Map<PickingData>();
			PickingData picking_data = *data;
			read_picking_buffer[backbuffer_index].Unmap();
			return picking_data;
		}

	private:

		GraphicsCoreDX12* gfx;
		StructuredBuffer<PickingData> write_picking_buffer;
		ReadbackBuffer read_picking_buffer[GraphicsCoreDX12::BackbufferCount()];
		ShaderBlob picker_blob;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> picker_rs;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> picker_pso;
	};
}