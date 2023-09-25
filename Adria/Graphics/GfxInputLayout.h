#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include "GfxFormat.h"

namespace adria
{
	enum class GfxInputClassification
	{
		PerVertexData,
		PerInstanceData
	};
	struct GfxInputLayout
	{
		static constexpr uint32 APPEND_ALIGNED_ELEMENT = ~0u;
		struct GfxInputElement
		{
			std::string semantic_name;
			uint32 semantic_index = 0;
			GfxFormat format = GfxFormat::UNKNOWN; 
			uint32 input_slot = 0;
			uint32 aligned_byte_offset = APPEND_ALIGNED_ELEMENT;
			GfxInputClassification input_slot_class = GfxInputClassification::PerVertexData;
		};
		std::vector<GfxInputElement> elements;
	};
	inline void ConvertInputLayout(GfxInputLayout const& input_layout, std::vector<D3D12_INPUT_ELEMENT_DESC>& element_descs)
	{
		element_descs.resize(input_layout.elements.size());
		for (uint32 i = 0; i < element_descs.size(); ++i)
		{
			GfxInputLayout::GfxInputElement const& element = input_layout.elements[i];
			D3D12_INPUT_ELEMENT_DESC desc{};
			desc.AlignedByteOffset = element.aligned_byte_offset;
			desc.Format = ConvertGfxFormat(element.format);
			desc.InputSlot = element.input_slot;
			switch (element.input_slot_class)
			{
			case GfxInputClassification::PerVertexData:
				desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				break;
			case GfxInputClassification::PerInstanceData:
				desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
				break;
			}
			desc.InstanceDataStepRate = 0;
			if (desc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA) desc.InstanceDataStepRate = 1;
			desc.SemanticIndex = element.semantic_index;
			desc.SemanticName = element.semantic_name.c_str();
			element_descs[i] = desc;
		}
	}
}