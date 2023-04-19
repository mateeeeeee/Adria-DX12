#include "AccelerationStructure.h"
#include "Components.h"
#include "Graphics/GfxBuffer.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxCommandList.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Utilities/Timer.h"
#include "Logging/Logger.h"

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
			rt_instance.instance_id = instance_id++; //#todo tmp, fix this
			rt_instance.instance_mask = 0xff;
			//
			const auto T = XMMatrixTranspose(instance.world_transform);
			memcpy(rt_instance.transform, &T, sizeof(T));
		}
	}

	void AccelerationStructure::Build() //add build flag options
	{
		if (blases.empty()) return;
		BuildBottomLevels();
		for(auto& rt_instance : rt_instances) rt_instance.blas = blases[rt_instance.instance_id].get();
		BuildTopLevel();
	}

	GfxRayTracingTLAS const& AccelerationStructure::GetTLAS() const
	{
		return *tlas;
	}

	void AccelerationStructure::BuildBottomLevels()
	{
		GfxCommandList* cmd_list = gfx->GetGraphicsCommandList();

		std::span<GfxRayTracingGeometry> geometry_span(rt_geometries);
		for (size_t i = 0; i < blases.size(); ++i)
		{
			blases[i] = std::make_unique<GfxRayTracingBLAS>(gfx, geometry_span.subspan(i, 1), GfxRayTracingASFlag_PreferFastTrace);

			//#todo improve this!!
			cmd_list->UavBarrier(blases[i]->GetBuffer());
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
		GfxCommandList* cmd_list = gfx->GetGraphicsCommandList();

		tlas = std::make_unique<GfxRayTracingTLAS>(gfx, rt_instances, GfxRayTracingASFlag_PreferFastTrace);

		cmd_list->UavBarrier(tlas->GetBuffer());
		cmd_list->FlushBarriers();
		cmd_list->Signal(build_fence, build_fence_value);
		cmd_list->End();
		cmd_list->Submit();
		cmd_list->Begin();

		build_fence.Wait(build_fence_value);
		++build_fence_value;
	}
}

