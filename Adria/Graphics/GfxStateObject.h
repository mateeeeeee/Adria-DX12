#pragma once
#include "d3dx12.h"
#include "GfxDefines.h"

namespace adria
{
	class GfxDevice;

	enum class GfxStateObjectType
	{
		Collection,
		RayTracingPipeline,
		Executable
	};

	class GfxStateObject
	{
		friend class GfxStateObjectBuilder;
		friend class GfxCommandList;
		friend class GfxRayTracingShaderTable;

	public:
		bool IsValid() const { return d3d12_so != nullptr; }

	private:
		Ref<ID3D12StateObject> d3d12_so;

	private:
		explicit GfxStateObject(ID3D12StateObject* so)
		{
			d3d12_so.Attach(so);
		}
	};

	class GfxStateObjectBuilder
	{
		static constexpr uint64 MAX_SUBOBJECT_DESC_SIZE = sizeof(D3D12_HIT_GROUP_DESC);
	public:
		explicit GfxStateObjectBuilder(uint64 max_subobjects) : max_subobjects(max_subobjects), num_subobjects(0u), subobjects(max_subobjects), subobject_data(max_subobjects* MAX_SUBOBJECT_DESC_SIZE) {}

		template<typename SubObjectDesc>
		void AddSubObject(SubObjectDesc const& desc)
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
			else if constexpr(std::is_same_v<SubObjectDesc, D3D12_WORK_GRAPH_DESC>)
				return AddSubObject(&desc, sizeof(desc), D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH);
			else
				return nullptr;
		}

		GfxStateObject* CreateStateObject(GfxDevice* gfx, GfxStateObjectType type = GfxStateObjectType::RayTracingPipeline);

	private:
		std::vector<uint8> subobject_data;
		std::vector<D3D12_STATE_SUBOBJECT> subobjects;
		uint64 const max_subobjects;
		uint64 num_subobjects;

	private:
		void AddSubObject(void const* desc, uint64 desc_size, D3D12_STATE_SUBOBJECT_TYPE type)
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
		}
	};
}