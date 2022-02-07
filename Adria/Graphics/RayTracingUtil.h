#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "../Core/Macros.h"
#include "../Core/Definitions.h"
#include "LinearUploadBuffer.h"

namespace adria
{
	using ShaderIdentifier = uint8[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];

	class ShaderRecord
	{
		friend class ShaderTable;

	public:
		explicit ShaderRecord(void const* _shader_id, void* _local_root_args = nullptr, uint32 _local_root_args_size = 0)
			: local_root_args(_local_root_args), local_root_args_size(_local_root_args_size)
		{
			memcpy(shader_id, _shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
		}

	private:
		ShaderIdentifier shader_id;
		void* local_root_args;
		uint32 local_root_args_size;
	};

	class ShaderTable
	{
	public:

		ShaderTable(ID3D12Device5* device, uint32 total_shader_records)
			: shader_record_size((uint32)Align(sizeof(ShaderRecord), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT)),
			  upload_buffer(device, total_shader_records * shader_record_size)
		{
			shader_records.reserve(total_shader_records);
		}

		void AddShaderRecord(ShaderRecord const& shader_record)
		{
			DynamicAllocation allocation = upload_buffer.Allocate(shader_record_size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

			allocation.Update(shader_record);

			shader_records.push_back(shader_record);
		}

		D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetRangeAndStride() const
		{
			D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE result = { };
			result.StartAddress = upload_buffer.GPUAddress();
			result.SizeInBytes = shader_records.size() * shader_record_size;
			result.StrideInBytes = shader_record_size;

			return result;
		}

		D3D12_GPU_VIRTUAL_ADDRESS_RANGE GetRange(uint64 element) const
		{
			ADRIA_ASSERT(element < shader_records.size());

			D3D12_GPU_VIRTUAL_ADDRESS_RANGE result = { };
			result.StartAddress = upload_buffer.GPUAddress() + shader_record_size * element;
			result.SizeInBytes = shader_record_size;
			return result;
		}


	private:
		std::vector<ShaderRecord> shader_records;
		uint32 shader_record_size;
		LinearUploadBuffer upload_buffer;
	};

	class StateObjectBuilder
	{
		static constexpr uint64 MAX_SUBOBJECT_DESC_SIZE = sizeof(D3D12_HIT_GROUP_DESC);
	public:

		explicit StateObjectBuilder(uint64 max_subobjects) : max_subobjects(max_subobjects), num_subobjects(0u), subobjects(max_subobjects), subobject_data(max_subobjects * MAX_SUBOBJECT_DESC_SIZE)
		{
		}

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
			BREAK_IF_FAILED(device->CreateStateObject(&desc, IID_PPV_ARGS(&state_obj)));
			return state_obj;
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

		void BuildDescription(D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc)
		{
			desc.Type = type;
			desc.NumSubobjects = static_cast<uint32>(num_subobjects);
			desc.pSubobjects = num_subobjects ? subobjects.data() : nullptr;
		}

	};

}