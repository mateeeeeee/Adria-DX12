#include "AccelerationStructure.h"
#include "Components.h"
#include "../Graphics/GfxBuffer.h"
#include "../Graphics/GfxDevice.h"
#include "../Graphics/GfxCommandList.h"
#include "../Graphics/GfxLinearDynamicAllocator.h"
#include "../Utilities/Timer.h"
#include "../Logging/Logger.h"

namespace adria
{

	AccelerationStructure::AccelerationStructure(GfxDevice* gfx) : gfx(gfx)
	{
		build_fence.Create(gfx, "Build Fence");
		++build_fence_value;
	}

	void AccelerationStructure::AddInstance(Mesh const& submesh, Transform const& transform, bool is_transparent /*= false*/)
	{
		auto dynamic_allocator = gfx->GetDynamicAllocator();

		GfxDynamicAllocation transform_alloc = dynamic_allocator->Allocate(sizeof(DirectX::XMMATRIX), D3D12_RAYTRACING_TRANSFORM3X4_BYTE_ALIGNMENT);
		transform_alloc.Update(XMMatrixTranspose(transform.current_transform));

		D3D12_RAYTRACING_GEOMETRY_DESC geo_desc{};
		geo_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geo_desc.Triangles.Transform3x4 = NULL;
		geo_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(CompleteVertex);
		geo_desc.Triangles.VertexBuffer.StartAddress = submesh.vertex_buffer->GetGPUAddress() + geo_desc.Triangles.VertexBuffer.StrideInBytes * submesh.base_vertex_location;
		geo_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geo_desc.Triangles.VertexCount = submesh.vertex_count;
		geo_desc.Triangles.IndexFormat = ConvertGfxFormat(submesh.index_buffer->GetDesc().format);
		geo_desc.Triangles.IndexBuffer = submesh.index_buffer->GetGPUAddress() + submesh.start_index_location * (submesh.index_buffer->GetDesc().stride);
		geo_desc.Triangles.IndexCount = submesh.indices_count;
		geo_desc.Flags = is_transparent ? D3D12_RAYTRACING_GEOMETRY_FLAG_NONE : D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geo_descs.push_back(geo_desc);
		geo_transforms.push_back(transform.current_transform);
	}

	void AccelerationStructure::Build() //add build flag options
	{
		BuildBottomLevels();
		BuildTopLevel();
	}

	GfxBuffer const* AccelerationStructure::GetTLAS() const
	{
		return tlas.get();
	}

	void AccelerationStructure::BuildBottomLevels()
	{
		ID3D12Device5* device = gfx->GetDevice();
		GfxCommandList* cmd_list = gfx->GetCommandList(GfxCommandListType::Graphics);

		std::unique_ptr<GfxBuffer> scratch_buffer = nullptr;

		blases.resize(geo_descs.size());
		for (size_t i = 0; i < blases.size(); ++i)
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bl_prebuild_info{};
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
			inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
			inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			inputs.NumDescs = 1;
			inputs.pGeometryDescs = &geo_descs[i];
			device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &bl_prebuild_info);
			ADRIA_ASSERT(bl_prebuild_info.ResultDataMaxSizeInBytes > 0);

			GfxBufferDesc scratch_buffer_desc{};
			scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
			scratch_buffer_desc.size = bl_prebuild_info.ScratchDataSizeInBytes;
			scratch_buffer = std::make_unique<GfxBuffer>(gfx, scratch_buffer_desc);

			GfxBufferDesc result_buffer_desc{};
			result_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource;
			result_buffer_desc.size = bl_prebuild_info.ResultDataMaxSizeInBytes;
			result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
			result_buffer_desc.stride = 4;
			blases[i] = std::make_unique<GfxBuffer>(gfx, result_buffer_desc);

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_desc{};
			blas_desc.Inputs = inputs;
			blas_desc.DestAccelerationStructureData = blases[i]->GetGPUAddress();
			blas_desc.ScratchAccelerationStructureData = scratch_buffer->GetGPUAddress();
			cmd_list->GetNative()->BuildRaytracingAccelerationStructure(&blas_desc, 0, nullptr);
			cmd_list->UavBarrier(blases[i].get());
			cmd_list->FlushBarriers();

			cmd_list->Signal(build_fence, build_fence_value);
			cmd_list->End();
			cmd_list->Submit();
			cmd_list->Begin();

			build_fence.Wait(build_fence_value);
			++build_fence_value;
		}
	}

	void AccelerationStructure::BuildTopLevel()
	{
		ID3D12Device5* device = gfx->GetDevice();
		GfxCommandList* cmd_list = gfx->GetCommandList(GfxCommandListType::Graphics);

		struct TLAccelerationStructureBuffers
		{
			std::unique_ptr<GfxBuffer> instance_buffer;
			std::unique_ptr<GfxBuffer> scratch_buffer;
		} tlas_buffers{};

		// First, get the size of the TLAS buffers and create them
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		inputs.NumDescs = (UINT)blases.size();
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tl_prebuild_info;
		device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &tl_prebuild_info);
		ADRIA_ASSERT(tl_prebuild_info.ResultDataMaxSizeInBytes > 0);

		GfxBufferDesc scratch_buffer_desc{};
		scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
		scratch_buffer_desc.size = tl_prebuild_info.ScratchDataSizeInBytes;
		tlas_buffers.scratch_buffer = std::make_unique<GfxBuffer>(gfx, scratch_buffer_desc);

		GfxBufferDesc result_buffer_desc{};
		result_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource;
		result_buffer_desc.size = tl_prebuild_info.ResultDataMaxSizeInBytes;
		result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
		tlas = std::make_unique<GfxBuffer>(gfx, result_buffer_desc);

		GfxBufferDesc instance_buffer_desc{};
		instance_buffer_desc.bind_flags = GfxBindFlag::None;
		instance_buffer_desc.size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * blases.size();
		instance_buffer_desc.resource_usage = GfxResourceUsage::Upload;
		tlas_buffers.instance_buffer = std::make_unique<GfxBuffer>(gfx, instance_buffer_desc);

		D3D12_RAYTRACING_INSTANCE_DESC* p_instance_desc = tlas_buffers.instance_buffer->GetMappedData<D3D12_RAYTRACING_INSTANCE_DESC>();
		for (size_t i = 0; i < blases.size(); ++i)
		{
			p_instance_desc[i].InstanceID = i;
			p_instance_desc[i].InstanceContributionToHitGroupIndex = 0;
			p_instance_desc[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			const auto T = XMMatrixTranspose(geo_transforms[i]); //maybe transpose
			memcpy(p_instance_desc[i].Transform, &T, sizeof(p_instance_desc->Transform));
			p_instance_desc[i].AccelerationStructure = blases[i]->GetGPUAddress();
			p_instance_desc[i].InstanceMask = 0xFF;
		}
		tlas_buffers.instance_buffer->Unmap();

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_desc{};
		tlas_desc.Inputs = inputs;
		tlas_desc.Inputs.InstanceDescs = tlas_buffers.instance_buffer->GetGPUAddress();
		tlas_desc.DestAccelerationStructureData = tlas->GetGPUAddress();
		tlas_desc.ScratchAccelerationStructureData = tlas_buffers.scratch_buffer->GetGPUAddress();
		cmd_list->GetNative()->BuildRaytracingAccelerationStructure(&tlas_desc, 0, nullptr);
		cmd_list->UavBarrier(tlas.get());

		cmd_list->Signal(build_fence, build_fence_value);
		cmd_list->End();
		cmd_list->Submit();
		cmd_list->Begin();

		build_fence.Wait(build_fence_value);
		++build_fence_value;

		tlas->CreateSRV();
	}
}

