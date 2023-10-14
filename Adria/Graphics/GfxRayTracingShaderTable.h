#pragma once
#include <d3d12.h>
#include <vector>
#include <memory.h>
#include "GfxLinearDynamicAllocator.h"
#include "GfxRingDynamicAllocator.h"
#include "GfxDefines.h"
#include "Core/CoreTypes.h"
#include "Utilities/StringUtil.h"
#include "Utilities/AutoRefCountPtr.h"

namespace adria
{

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
		explicit GfxRayTracingShaderTable(ID3D12StateObject* state_object)
			: state_object(state_object)
		{
			GFX_CHECK_HR(state_object->QueryInterface(IID_PPV_ARGS(pso_info.GetAddressOf())));
		}

		void SetRayGenShader(char const* name, void* local_data = nullptr, uint32 data_size = 0)
		{
			void const* ray_gen_id = pso_info->GetShaderIdentifier(ToWideString(name).c_str());
			ray_gen_record.Init(ray_gen_id, local_data, data_size);
			ray_gen_record_size = (uint32)Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		}
		void AddMissShader(char const* name, uint32 i, void* local_data = nullptr, uint32 data_size = 0)
		{
			if (i >= (uint32)miss_shader_records.size())
			{
				miss_shader_records.resize(i + 1);
			}
			void const* miss_id = pso_info->GetShaderIdentifier(ToWideString(name).c_str());
			miss_shader_records[i].Init(miss_id, local_data, data_size);
			miss_shader_record_size = std::max(miss_shader_record_size, (uint32)Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
		}
		void AddHitGroup(char const* name, uint32 i, void* local_data = nullptr, uint32 data_size = 0)
		{
			if (i >= (uint32)hit_group_records.size())
			{
				hit_group_records.resize(i + 1);
			}
			void const* miss_id = pso_info->GetShaderIdentifier(ToWideString(name).c_str());
			hit_group_records[i].Init(miss_id, local_data, data_size);
			hit_group_record_size = std::max(hit_group_record_size, (uint32)Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + data_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));
		}

		void Commit(GfxLinearDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
		{
			CommitImpl(allocator, desc);
		}
		void Commit(GfxRingDynamicAllocator& allocator, D3D12_DISPATCH_RAYS_DESC& desc)
		{
			CommitImpl(allocator, desc);
		}

	private:
		ID3D12StateObject* state_object;
		ArcPtr<ID3D12StateObjectProperties> pso_info = nullptr;
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

	class GfxStateObjectBuilder
	{
		static constexpr uint64 MAX_SUBOBJECT_DESC_SIZE = sizeof(D3D12_HIT_GROUP_DESC);
	public:

		explicit GfxStateObjectBuilder(uint64 max_subobjects) : max_subobjects(max_subobjects), num_subobjects(0u), subobjects(max_subobjects), subobject_data(max_subobjects * MAX_SUBOBJECT_DESC_SIZE)
		{}

		template<typename SubObjectDesc>
		[[maybe_unused]] D3D12_STATE_SUBOBJECT const* AddSubObject(SubObjectDesc const& desc)
		{
			if constexpr (std::is_same_v<SubObjectDesc, D3D12_STATE_OBJECT_CONFIG>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_GLOBAL_ROOT_SIGNATURE>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_LOCAL_ROOT_SIGNATURE>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_NODE_MASK>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_DXIL_LIBRARY_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_EXISTING_COLLECTION_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_RAYTRACING_SHADER_CONFIG>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_RAYTRACING_PIPELINE_CONFIG>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG);
			else if constexpr (std::is_same_v<SubObjectDesc, D3D12_HIT_GROUP_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP);
			else 
				return nullptr;
		}

		ID3D12StateObject* CreateStateObject(ID3D12Device5* device, D3D12_STATE_OBJECT_TYPE type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE)
		{
			D3D12_STATE_OBJECT_DESC desc{};
			BuildDescription(type, desc);
			ID3D12StateObject* state_obj = nullptr;
			HRESULT hr = device->CreateStateObject(&desc, IID_PPV_ARGS(&state_obj));
			GFX_CHECK_HR(hr);
			return state_obj;
		}

		D3D12_STATE_OBJECT_DESC GetDesc(D3D12_STATE_OBJECT_TYPE type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) const
		{
			D3D12_STATE_OBJECT_DESC desc{};
			BuildDescription(type, desc);
			return desc;
		}

	private:
		std::vector<uint8> subobject_data;
		std::vector<D3D12_STATE_SUBOBJECT> subobjects;
		uint64 const max_subobjects;
		uint64 num_subobjects;

	private:
		
		D3D12_STATE_SUBOBJECT const* AddSubObject(void const* desc, uint64 desc_size, D3D12_STATE_SUBOBJECT_TYPE type)
		{
			ADRIA_ASSERT(desc != nullptr);
			ADRIA_ASSERT(desc_size > 0);
			ADRIA_ASSERT(type < D3D12_STATE_SUBOBJECT_TYPE_MAX_VALID);
			ADRIA_ASSERT(desc_size <= MAX_SUBOBJECT_DESC_SIZE);
			ADRIA_ASSERT(num_subobjects < max_subobjects);

			const uint64 subobject_offset = num_subobjects * MAX_SUBOBJECT_DESC_SIZE;
			memcpy(subobject_data.data() + subobject_offset, desc, desc_size);

			D3D12_STATE_SUBOBJECT& subobject = subobjects[num_subobjects];
			subobject.Type = type;
			subobject.pDesc = subobject_data.data() + subobject_offset;
			++num_subobjects;

			return &subobject;
		}

		void BuildDescription(D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc) const
		{
			desc.Type = type;
			desc.NumSubobjects = static_cast<uint32>(num_subobjects);
			desc.pSubobjects = num_subobjects ? subobjects.data() : nullptr;
		}

	};
}