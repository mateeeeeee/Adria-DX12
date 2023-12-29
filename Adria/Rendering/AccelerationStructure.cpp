#include "AccelerationStructure.h"
#include "Components.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"

namespace adria
{

	AccelerationStructure::AccelerationStructure(GfxDevice* gfx) : gfx(gfx)
	{
		build_fence.Create(gfx, "Build Fence");
		++build_fence_value;
	}

	void AccelerationStructure::AddInstance(Mesh const& mesh)
	{
		auto dynamic_allocator = gfx->GetDynamicAllocator();
		blases.resize(mesh.instances.size());
		uint32 instance_id = 0;

		GfxBuffer* geometry_buffer = g_GeometryBufferCache.GetGeometryBuffer(mesh.geometry_buffer_handle);
		for (SubMeshInstance const& instance : mesh.instances)
		{
			SubMeshGPU const& submesh = mesh.submeshes[instance.submesh_index];
			Material const& material = mesh.materials[submesh.material_index];

			GfxRayTracingGeometry& rt_geometry = rt_geometries.emplace_back();
			rt_geometry.vertex_buffer = geometry_buffer;
			rt_geometry.vertex_buffer_offset = submesh.positions_offset;
			rt_geometry.vertex_format = GfxFormat::R32G32B32_FLOAT;
			rt_geometry.vertex_stride = GetGfxFormatStride(rt_geometry.vertex_format);
			rt_geometry.vertex_count = submesh.vertices_count;

			rt_geometry.index_buffer = geometry_buffer;
			rt_geometry.index_buffer_offset = submesh.indices_offset;
			rt_geometry.index_count = submesh.indices_count;
			rt_geometry.index_format = GfxFormat::R32_UINT;
			rt_geometry.opaque = material.alpha_mode == MaterialAlphaMode::Opaque;

			GfxRayTracingInstance& rt_instance = rt_instances.emplace_back();
			rt_instance.flags = GfxRayTracingInstanceFlag_None;
			rt_instance.instance_id = instance_id++; //#todo temporary
			rt_instance.instance_mask = 0xff;
			const auto T = XMMatrixTranspose(instance.world_transform);
			memcpy(rt_instance.transform, &T, sizeof(T));
		}
	}

	void AccelerationStructure::Build()
	{
		if (blases.empty()) return;
		BuildBottomLevels();
		for(auto& rt_instance : rt_instances) rt_instance.blas = blases[rt_instance.instance_id].get();
		BuildTopLevel();
		tlas_srv = gfx->CreateBufferSRV(&tlas->GetBuffer());
	}

	int32 AccelerationStructure::GetTLASIndex() const
	{
		GfxDescriptor tlas_srv_gpu = gfx->AllocateDescriptorsGPU();
		gfx->CopyDescriptors(1, tlas_srv_gpu, tlas_srv);
		return (int32)tlas_srv_gpu.GetIndex();
	}

	void AccelerationStructure::BuildBottomLevels()
	{
		GfxCommandList* cmd_list = gfx->GetCommandList();

		std::span<GfxRayTracingGeometry> geometry_span(rt_geometries);
		for (uint64 i = 0; i < blases.size(); ++i)
		{
			blases[i] = gfx->CreateRayTracingBLAS(geometry_span.subspan(i, 1), GfxRayTracingASFlag_PreferFastTrace);
		}
		cmd_list->Signal(build_fence, build_fence_value);
		cmd_list->End();
		cmd_list->Submit();
	}

	void AccelerationStructure::BuildTopLevel()
	{
		GfxCommandList* cmd_list = gfx->GetCommandList();
		cmd_list->Begin();

		build_fence.Wait(build_fence_value);
		++build_fence_value;

		tlas = gfx->CreateRayTracingTLAS(rt_instances, GfxRayTracingASFlag_PreferFastTrace);

		cmd_list->Signal(build_fence, build_fence_value);
		cmd_list->End();
		cmd_list->Submit();

		build_fence.Wait(build_fence_value);
		++build_fence_value;

		cmd_list->Begin();
	}
}

