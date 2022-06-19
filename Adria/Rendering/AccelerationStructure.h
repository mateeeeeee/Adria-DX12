#pragma once
#include <vector>
#include "../Graphics/Buffer.h"
#include "Components.h"

namespace adria
{

	class AccelerationStructure
	{
	public:
		AccelerationStructure(GraphicsDevice* gfx) : gfx(gfx)
		{}

		void AddInstance(Mesh const& mesh, Transform const& transform, bool is_transparent = false)
		{
			auto dynamic_allocator = gfx->GetDynamicAllocator();

			DynamicAllocation transform_alloc = dynamic_allocator->Allocate(sizeof(DirectX::XMMATRIX), D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT);
			transform_alloc.Update(transform.current_transform); //maybe transpose?

			D3D12_RAYTRACING_GEOMETRY_DESC geo_desc{};
			geo_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			geo_desc.Triangles.Transform3x4 = transform_alloc.gpu_address;
			geo_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(CompleteVertex);
			geo_desc.Triangles.VertexBuffer.StartAddress = mesh.vertex_buffer->GetGPUAddress() + geo_desc.Triangles.VertexBuffer.StrideInBytes * mesh.base_vertex_location;
			geo_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			geo_desc.Triangles.VertexCount = mesh.vertex_count;
			geo_desc.Triangles.IndexFormat = mesh.index_buffer->GetDesc().format;
			geo_desc.Triangles.IndexBuffer = mesh.index_buffer->GetGPUAddress() + mesh.start_index_location * (mesh.index_buffer->GetDesc().stride);
			geo_desc.Triangles.IndexCount = mesh.indices_count;
			geo_desc.Flags = is_transparent ? D3D12_RAYTRACING_GEOMETRY_FLAG_NONE : D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
			geo_descs.push_back(geo_desc);
		}

		void Build() //add build flag options 
		{
			BuildBottomLevel();
			BuildTopLevel();
		}

		Buffer const* GetTLAS() const { return tlas.get(); }

	private:
		GraphicsDevice* gfx;
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geo_descs;
		std::unique_ptr<Buffer> blas;
		std::unique_ptr<Buffer> tlas;

	private:
		void BuildBottomLevel()
		{
			ID3D12Device5* device = gfx->GetDevice();
			ID3D12GraphicsCommandList4* cmd_list = gfx->GetDefaultCommandList();

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bl_prebuild_info{};
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
			inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			inputs.NumDescs = static_cast<uint32>(geo_descs.size());
			inputs.pGeometryDescs = geo_descs.data();
			device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &bl_prebuild_info);
			ADRIA_ASSERT(bl_prebuild_info.ResultDataMaxSizeInBytes > 0);

			struct BLAccelerationStructureBuffers
			{
				std::unique_ptr<Buffer> scratch_buffer;
				std::unique_ptr<Buffer> result_buffer;
			} blas_buffers{};

			BufferDesc scratch_buffer_desc{};
			scratch_buffer_desc.bind_flags = EBindFlag::UnorderedAccess;
			scratch_buffer_desc.size = bl_prebuild_info.ScratchDataSizeInBytes;
			blas_buffers.scratch_buffer = std::make_unique<Buffer>(gfx, scratch_buffer_desc);

			BufferDesc result_buffer_desc{};
			result_buffer_desc.bind_flags = EBindFlag::UnorderedAccess;
			result_buffer_desc.size = bl_prebuild_info.ResultDataMaxSizeInBytes;
			blas_buffers.result_buffer = std::make_unique<Buffer>(gfx, result_buffer_desc);

			// Create the bottom-level AS
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_desc{};
			blas_desc.Inputs = inputs;
			blas_desc.DestAccelerationStructureData = blas_buffers.result_buffer->GetGPUAddress();
			blas_desc.ScratchAccelerationStructureData = blas_buffers.scratch_buffer->GetGPUAddress();
			cmd_list->BuildRaytracingAccelerationStructure(&blas_desc, 0, nullptr);

			D3D12_RESOURCE_BARRIER uav_barrier{};
			uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uav_barrier.UAV.pResource = blas_buffers.result_buffer->GetNative();
			cmd_list->ResourceBarrier(1, &uav_barrier);
			blas = std::move(blas_buffers.result_buffer);

		}
		void BuildTopLevel()
		{
			ID3D12Device5* device = gfx->GetDevice();
			ID3D12GraphicsCommandList4* cmd_list = gfx->GetDefaultCommandList();

			struct TLAccelerationStructureBuffers
			{
				std::unique_ptr<Buffer> instance_buffer;
				std::unique_ptr<Buffer> scratch_buffer;
				std::unique_ptr<Buffer> result_buffer;
			} tlas_buffers{};

			// First, get the size of the TLAS buffers and create them
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
			inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			inputs.NumDescs = 1;
			inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tl_prebuild_info;
			device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &tl_prebuild_info);
			ADRIA_ASSERT(tl_prebuild_info.ResultDataMaxSizeInBytes > 0);

			BufferDesc scratch_buffer_desc{};
			scratch_buffer_desc.bind_flags = EBindFlag::UnorderedAccess;
			scratch_buffer_desc.size = tl_prebuild_info.ScratchDataSizeInBytes;
			tlas_buffers.scratch_buffer = std::make_unique<Buffer>(gfx, scratch_buffer_desc);

			BufferDesc result_buffer_desc{};
			result_buffer_desc.bind_flags = EBindFlag::UnorderedAccess;
			result_buffer_desc.size = tl_prebuild_info.ResultDataMaxSizeInBytes;
			tlas_buffers.result_buffer = std::make_unique<Buffer>(gfx, result_buffer_desc);

			BufferDesc instance_buffer_desc{};
			instance_buffer_desc.bind_flags = EBindFlag::None;
			instance_buffer_desc.size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
			instance_buffer_desc.resource_usage = EResourceUsage::Upload;
			tlas_buffers.instance_buffer = std::make_unique<Buffer>(gfx, instance_buffer_desc);

			D3D12_RAYTRACING_INSTANCE_DESC* p_instance_desc = tlas_buffers.instance_buffer->GetMappedData<D3D12_RAYTRACING_INSTANCE_DESC>();
			p_instance_desc->InstanceID = 0;
			p_instance_desc->InstanceContributionToHitGroupIndex = 0;
			p_instance_desc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			static const auto I = DirectX::XMMatrixIdentity();
			memcpy(p_instance_desc->Transform, &I, sizeof(p_instance_desc->Transform));
			p_instance_desc->AccelerationStructure = blas->GetGPUAddress();
			p_instance_desc->InstanceMask = 0xFF;
			tlas_buffers.instance_buffer->Unmap();

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_desc{};
			tlas_desc.Inputs = inputs;
			tlas_desc.Inputs.InstanceDescs = tlas_buffers.instance_buffer->GetGPUAddress();
			tlas_desc.DestAccelerationStructureData = tlas_buffers.result_buffer->GetGPUAddress();
			tlas_desc.ScratchAccelerationStructureData = tlas_buffers.scratch_buffer->GetGPUAddress();
			cmd_list->BuildRaytracingAccelerationStructure(&tlas_desc, 0, nullptr);

			D3D12_RESOURCE_BARRIER uav_barrier{};
			uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			uav_barrier.UAV.pResource = tlas_buffers.result_buffer->GetNative();
			cmd_list->ResourceBarrier(1, &uav_barrier);

			tlas = std::move(tlas_buffers.result_buffer);
		}
	};
}