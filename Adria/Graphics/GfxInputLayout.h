#pragma once
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
}