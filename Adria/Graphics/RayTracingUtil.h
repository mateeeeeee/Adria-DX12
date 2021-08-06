#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "../Core/Macros.h"
#include "../Core/Definitions.h"

namespace adria
{
	struct HitGroupRecord
	{
		u8 shader_id[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES]{};

		HitGroupRecord() = default;
		explicit HitGroupRecord(void const* _data)
		{
			memcpy(shader_id, _data, sizeof(shader_id));
		}
	};
	static_assert(sizeof(HitGroupRecord) % D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT == 0);

	class StateObjectBuilder
	{
		static constexpr u64 MAX_SUBOBJECT_DESC_SIZE = sizeof(D3D12_HIT_GROUP_DESC);
	public:

		explicit StateObjectBuilder(u64 max_subobjects = 12) : max_subobjects(max_subobjects), num_subobjects(0u), subobjects(max_subobjects), subobject_data(max_subobjects * MAX_SUBOBJECT_DESC_SIZE)
		{
		}

		template<typename SubObjectDesc>
		D3D12_STATE_SUBOBJECT const* AddSubObject(SubObjectDesc const& desc)
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

		ID3D12StateObject* CreateStateObject(ID3D12Device5* device, D3D12_STATE_OBJECT_TYPE type)
		{
			D3D12_STATE_OBJECT_DESC desc{};
			BuildDescription(type, desc);
			ID3D12StateObject* state_obj = nullptr;
			BREAK_IF_FAILED(device->CreateStateObject(&desc, IID_PPV_ARGS(&state_obj)));
			return state_obj;
		}

	private:
		std::vector<u8> subobject_data;
		std::vector<D3D12_STATE_SUBOBJECT> subobjects;
		u64 const max_subobjects;
		u64 num_subobjects;

	private:
		D3D12_STATE_SUBOBJECT const* AddSubObject(void const* desc, u64 desc_size, D3D12_STATE_SUBOBJECT_TYPE type)
		{
			ADRIA_ASSERT(desc != nullptr);
			ADRIA_ASSERT(desc_size > 0);
			ADRIA_ASSERT(type < D3D12_STATE_SUBOBJECT_TYPE_MAX_VALID);
			ADRIA_ASSERT(desc_size <= MAX_SUBOBJECT_DESC_SIZE);
			ADRIA_ASSERT(num_subobjects < max_subobjects);

			const u64 subobject_offset = num_subobjects * MAX_SUBOBJECT_DESC_SIZE;
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
			desc.NumSubobjects = static_cast<u32>(num_subobjects);
			desc.pSubobjects = num_subobjects ? subobjects.data() : nullptr;
		}

	};

}