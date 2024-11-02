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
		static constexpr Uint32 APPEND_ALIGNED_ELEMENT = ~0u;
		struct GfxInputElement
		{
			std::string semantic_name;
			Uint32 semantic_index = 0;
			GfxFormat format = GfxFormat::UNKNOWN; 
			Uint32 input_slot = 0;
			Uint32 aligned_byte_offset = APPEND_ALIGNED_ELEMENT;
			GfxInputClassification input_slot_class = GfxInputClassification::PerVertexData;
		};
		std::vector<GfxInputElement> elements;
	};
}