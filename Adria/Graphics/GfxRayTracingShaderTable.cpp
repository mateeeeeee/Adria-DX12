#include "GfxRayTracingShaderTable.h"
#include "GfxStateObject.h"
#include "GfxLinearDynamicAllocator.h"
#include "GfxRingDynamicAllocator.h"
#include "Utilities/StringUtil.h"

namespace adria
{

	GfxRayTracingShaderTable::GfxRayTracingShaderTable(GfxStateObject* state_object)
	{
		GFX_CHECK_HR(state_object->d3d12_so->QueryInterface(IID_PPV_ARGS(pso_info.GetAddressOf())));
	}

	void GfxRayTracingShaderTable::SetRayGenShader(char const* name, void* local_data /*= nullptr*/, uint32 data_size /*= 0*/)
	{
		void const* ray_gen_id = pso_info->GetShaderIdentifier(ToWideString(name).c_str());
		ray_gen_record.Init(ray_gen_id, local_data, data_size);
		ray_gen_record_size = (uint32)Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	}

	void GfxRayTracingShaderTable::AddMissShader(char const* name, uint32 i, void* local_data /*= nullptr*/, uint32 data_size /*= 0*/)
	{
		if (i >= (uint32)miss_shader_records.size())
		{
			miss_shader_records.resize(i + 1);
		}
		void const* miss_id = pso_info->GetShaderIdentifier(ToWideString(name).c_str());
		miss_shader_records[i].Init(miss_id, local_data, data_size);
		miss_shader_record_size = std::max(miss_shader_record_size, (uint32)Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
	}

	void GfxRayTracingShaderTable::AddHitGroup(char const* name, uint32 i, void* local_data /*= nullptr*/, uint32 data_size /*= 0*/)
	{
		if (i >= (uint32)hit_group_records.size())
		{
			hit_group_records.resize(i + 1);
		}
		void const* miss_id = pso_info->GetShaderIdentifier(ToWideString(name).c_str());
		hit_group_records[i].Init(miss_id, local_data, data_size);
		hit_group_record_size = std::max(hit_group_record_size, (uint32)Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
	}

	void GfxRayTracingShaderTable::Commit(GfxLinearDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
	{
		CommitImpl(allocator, desc);
	}

	void GfxRayTracingShaderTable::Commit(GfxRingDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
	{
		CommitImpl(allocator, desc);
	}

}

