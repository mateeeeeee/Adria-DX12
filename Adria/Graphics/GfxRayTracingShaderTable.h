#pragma once
#include <vector>
#include "GfxDefines.h"
#include "GfxDynamicAllocation.h"

namespace adria
{
	class GfxStateObject;
	class GfxLinearDynamicAllocator;
	class GfxRingDynamicAllocator;
	
	class GfxRayTracingShaderTable
	{
		struct GfxShaderRecord
		{
			using ShaderIdentifier = uint8[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];

			GfxShaderRecord() = default;
			void Init(void const* _shader_id, void* _local_root_args = nullptr, uint32 _local_root_args_size = 0)
			{
				local_root_args_size = _local_root_args_size;
				memcpy(shader_id, _shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				local_root_args = std::make_unique<uint8[]>(local_root_args_size);
				memcpy(local_root_args.get(), _local_root_args, local_root_args_size);
			}

			ShaderIdentifier shader_id = {};
			std::unique_ptr<uint8[]> local_root_args = nullptr;
			uint32 local_root_args_size = 0;
		};

	public:
		explicit GfxRayTracingShaderTable(GfxStateObject* state_object);

		void SetRayGenShader(char const* name, void* local_data = nullptr, uint32 data_size = 0);
		void AddMissShader(char const* name, uint32 i, void* local_data = nullptr, uint32 data_size = 0);
		void AddHitGroup(char const* name, uint32 i, void* local_data = nullptr, uint32 data_size = 0);

		void Commit(GfxLinearDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc);
		void Commit(GfxRingDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc);

	private:
		Ref<ID3D12StateObjectProperties> pso_info = nullptr;
		GfxShaderRecord ray_gen_record;
		uint32 ray_gen_record_size = 0;
		std::vector<GfxShaderRecord> miss_shader_records;
		uint32 miss_shader_record_size = 0;
		std::vector<GfxShaderRecord> hit_group_records;
		uint32 hit_group_record_size = 0;

	private:

		template<typename TAllocator>
		void CommitImpl(TAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
		{
			uint32 total_size = 0;
			uint32 rg_section = ray_gen_record_size;
			uint32 rg_section_aligned = (uint32)Align(rg_section, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
			uint32 miss_section = miss_shader_record_size * (uint32)miss_shader_records.size();
			uint32 miss_section_aligned = (uint32)Align(miss_section, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
			uint32 hit_section = hit_group_record_size * (uint32)hit_group_records.size();
			uint32 hit_section_aligned = (uint32)Align(hit_section, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
			total_size = (uint32)Align(rg_section_aligned + miss_section_aligned + hit_section_aligned, 256);

			GfxDynamicAllocation allocation = allocator.Allocate(total_size, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

			uint8* p_start = (uint8*)allocation.cpu_address;
			uint8* p_data = p_start;

			memcpy(p_data, ray_gen_record.shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			memcpy(p_data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, ray_gen_record.local_root_args.get(), ray_gen_record.local_root_args_size);
			p_data += ray_gen_record_size;
			p_data = p_start + rg_section_aligned;

			for (auto const& r : miss_shader_records)
			{
				memcpy(p_data, r.shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(p_data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, r.local_root_args.get(), r.local_root_args_size);
				p_data += miss_shader_record_size;
			}
			p_data = p_start + rg_section_aligned + miss_section_aligned;

			for (auto const& r : hit_group_records)
			{
				memcpy(p_data, r.shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
				memcpy(p_data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, r.local_root_args.get(), r.local_root_args_size);
				p_data += hit_group_record_size;
			}

			desc.RayGenerationShaderRecord.StartAddress = allocation.gpu_address;
			desc.RayGenerationShaderRecord.SizeInBytes = rg_section;
			desc.MissShaderTable.StartAddress = allocation.gpu_address + rg_section_aligned;
			desc.MissShaderTable.SizeInBytes = miss_section;
			desc.MissShaderTable.StrideInBytes = miss_shader_record_size;
			desc.HitGroupTable.StartAddress = allocation.gpu_address + rg_section_aligned + miss_section_aligned;
			desc.HitGroupTable.SizeInBytes = hit_section;
			desc.HitGroupTable.StrideInBytes = hit_group_record_size;
		}
	};
}