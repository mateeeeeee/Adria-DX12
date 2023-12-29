#include "GfxRayTracingAS.h"
#include "GfxDevice.h"
#include "GfxCommandList.h"
#include "GfxBuffer.h"

namespace adria
{
	namespace
	{
		constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS ConvertASFlags(GfxRayTracingASFlags flags)
		{
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS d3d12_flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
			if (flags & GfxRayTracingASFlag_PreferFastTrace)
				d3d12_flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
			if(flags & GfxRayTracingASFlag_PreferFastBuild)
				d3d12_flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
			if(flags & GfxRayTracingASFlag_PerformUpdate)
				d3d12_flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
			if (flags & GfxRayTracingASFlag_AllowCompaction)
				d3d12_flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
			if (flags & GfxRayTracingASFlag_AllowUpdate)
				d3d12_flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
			if (flags & GfxRayTracingASFlag_MinimizeMemory)
				d3d12_flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_MINIMIZE_MEMORY;
			return d3d12_flags;
		}
		constexpr D3D12_RAYTRACING_INSTANCE_FLAGS ConvertInstanceFlags(GfxRayTracingInstanceFlags flags)
		{
			D3D12_RAYTRACING_INSTANCE_FLAGS d3d12_flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
			if (flags & GfxRayTracingInstanceFlag_ForceOpaque)
				d3d12_flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
			if (flags & GfxRayTracingInstanceFlag_ForceNoOpaque)
				d3d12_flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;
			if (flags & GfxRayTracingInstanceFlag_CullDisable)
				d3d12_flags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
			if (flags & GfxRayTracingInstanceFlag_FrontCCW)
				d3d12_flags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
			return d3d12_flags;
		}

		inline D3D12_RAYTRACING_GEOMETRY_DESC ConvertRayTracingGeometry(GfxRayTracingGeometry const& geometry)
		{
			D3D12_RAYTRACING_GEOMETRY_DESC d3d12_desc{};
			d3d12_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			d3d12_desc.Flags = geometry.opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
			d3d12_desc.Triangles.Transform3x4 = NULL;
			d3d12_desc.Triangles.VertexBuffer.StartAddress = geometry.vertex_buffer->GetGpuAddress() + geometry.vertex_buffer_offset;
			d3d12_desc.Triangles.VertexBuffer.StrideInBytes = geometry.vertex_stride;
			d3d12_desc.Triangles.VertexCount = geometry.vertex_count;
			d3d12_desc.Triangles.VertexFormat = ConvertGfxFormat(geometry.vertex_format);
			d3d12_desc.Triangles.IndexFormat = ConvertGfxFormat(geometry.index_format);
			d3d12_desc.Triangles.IndexCount = geometry.index_count;
			d3d12_desc.Triangles.IndexBuffer = geometry.index_buffer->GetGpuAddress() + geometry.index_buffer_offset;
			return d3d12_desc;
		}
	}

	GfxRayTracingBLAS::GfxRayTracingBLAS(GfxDevice* gfx, std::span<GfxRayTracingGeometry> geometries, GfxRayTracingASFlags flags)
	{
		std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geo_descs; geo_descs.reserve(geometries.size());
		for (auto&& geometry : geometries)	geo_descs.push_back(ConvertRayTracingGeometry(geometry));

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = ConvertASFlags(flags);
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

		inputs.NumDescs = (uint32)geo_descs.size();
		inputs.pGeometryDescs = geo_descs.data();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bl_prebuild_info{};
		gfx->GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &bl_prebuild_info);
		ADRIA_ASSERT(bl_prebuild_info.ResultDataMaxSizeInBytes > 0);

		GfxBufferDesc scratch_buffer_desc{};
		scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
		scratch_buffer_desc.size = bl_prebuild_info.ScratchDataSizeInBytes;
		scratch_buffer = gfx->CreateBuffer(scratch_buffer_desc);
		scratch_buffer->SetName("scratch buffer");

		GfxBufferDesc result_buffer_desc{};
		result_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource;
		result_buffer_desc.size = bl_prebuild_info.ResultDataMaxSizeInBytes;
		result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
		result_buffer_desc.stride = 4;
		result_buffer = gfx->CreateBuffer(result_buffer_desc);
		scratch_buffer->SetName("result buffer");

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blas_desc{};
		blas_desc.Inputs = inputs;
		blas_desc.DestAccelerationStructureData = result_buffer->GetGpuAddress();
		blas_desc.ScratchAccelerationStructureData = scratch_buffer->GetGpuAddress();

		GfxCommandList* cmd_list = gfx->GetCommandList();
		cmd_list->GetNative()->BuildRaytracingAccelerationStructure(&blas_desc, 0, nullptr);
	}

	GfxRayTracingBLAS::~GfxRayTracingBLAS() = default;

	uint64 GfxRayTracingBLAS::GetGpuAddress() const
	{
		return result_buffer->GetGpuAddress();
	}

	GfxRayTracingTLAS::GfxRayTracingTLAS(GfxDevice* gfx, std::span<GfxRayTracingInstance> instances, GfxRayTracingASFlags flags)
	{
		// First, get the size of the TLAS buffers and create them
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
		inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		inputs.Flags = ConvertASFlags(flags);
		inputs.NumDescs = (uint32)instances.size();
		inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tl_prebuild_info;
		gfx->GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &tl_prebuild_info);
		ADRIA_ASSERT(tl_prebuild_info.ResultDataMaxSizeInBytes > 0);

		GfxBufferDesc scratch_buffer_desc{};
		scratch_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess;
		scratch_buffer_desc.size = tl_prebuild_info.ScratchDataSizeInBytes;
		scratch_buffer = gfx->CreateBuffer(scratch_buffer_desc);

		GfxBufferDesc result_buffer_desc{};
		result_buffer_desc.bind_flags = GfxBindFlag::UnorderedAccess | GfxBindFlag::ShaderResource;
		result_buffer_desc.size = tl_prebuild_info.ResultDataMaxSizeInBytes;
		result_buffer_desc.misc_flags = GfxBufferMiscFlag::AccelStruct;
		result_buffer = gfx->CreateBuffer(result_buffer_desc);

		GfxBufferDesc instance_buffer_desc{};
		instance_buffer_desc.bind_flags = GfxBindFlag::None;
		instance_buffer_desc.size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instances.size();
		instance_buffer_desc.resource_usage = GfxResourceUsage::Upload;
		instance_buffer = gfx->CreateBuffer(instance_buffer_desc);

		D3D12_RAYTRACING_INSTANCE_DESC* p_instance_desc = instance_buffer->GetMappedData<D3D12_RAYTRACING_INSTANCE_DESC>();
		for (uint64 i = 0; i < instances.size(); ++i)
		{
			p_instance_desc[i].InstanceID = instances[i].instance_id;
			p_instance_desc[i].InstanceContributionToHitGroupIndex = 0;
			p_instance_desc[i].Flags = ConvertInstanceFlags(instances[i].flags);
			memcpy(p_instance_desc[i].Transform, &instances[i].transform, sizeof(p_instance_desc->Transform));
			p_instance_desc[i].AccelerationStructure = instances[i].blas->GetGpuAddress();
			p_instance_desc[i].InstanceMask = instances[i].instance_mask;
		}
		instance_buffer->Unmap();

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_desc{};
		tlas_desc.Inputs = inputs;
		tlas_desc.Inputs.InstanceDescs = instance_buffer->GetGpuAddress();
		tlas_desc.DestAccelerationStructureData = result_buffer->GetGpuAddress();
		tlas_desc.ScratchAccelerationStructureData = scratch_buffer->GetGpuAddress();

		GfxCommandList* cmd_list = gfx->GetCommandList();
		cmd_list->GetNative()->BuildRaytracingAccelerationStructure(&tlas_desc, 0, nullptr);
	}

	GfxRayTracingTLAS::~GfxRayTracingTLAS() = default;

	uint64 GfxRayTracingTLAS::GetGpuAddress() const
	{
		return result_buffer->GetGpuAddress();
	}

}

