#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include "../Core/Macros.h"
#include "../Core/Definitions.h"

namespace adria
{
	struct ShaderMacro
	{
		std::wstring name;
		std::wstring value;
	};

	enum class EShaderStage
	{
		VS,
		PS,
		HS,
		DS,
		GS,
		CS,
		LIB,
		MS,
		AS,
		ShaderCount
	};
	struct ShaderBlob
	{
		std::vector<uint8_t> bytecode;
		void* GetPointer() const
		{
			return !bytecode.empty() ? (void*)bytecode.data() : nullptr;
		}
		size_t GetLength() const
		{
			return bytecode.size();
		}
		operator D3D12_SHADER_BYTECODE() const
		{
			D3D12_SHADER_BYTECODE Bytecode{};
			Bytecode.pShaderBytecode = GetPointer();
			Bytecode.BytecodeLength = GetLength();
			return Bytecode;
		}
	};

	enum class EInputClassification
	{
		PerVertexData,
		PerInstanceData
	};
	struct InputLayout
	{
		static constexpr uint32 APPEND_ALIGNED_ELEMENT = ~0u; 
		struct InputElement
		{
			std::string semantic_name;
			uint32 semantic_index = 0;
			DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
			uint32 input_slot = 0;
			uint32 aligned_byte_offset = APPEND_ALIGNED_ELEMENT;
			EInputClassification input_slot_class = EInputClassification::PerVertexData;
		};
		std::vector<InputElement> elements;
	};

	inline void ConvertInputLayout(InputLayout const& input_layout, std::vector<D3D12_INPUT_ELEMENT_DESC>& element_descs)
	{
		element_descs.resize(input_layout.elements.size());
		for (uint32_t i = 0; i < element_descs.size(); ++i)
		{
			InputLayout::InputElement const& element = input_layout.elements[i];
			D3D12_INPUT_ELEMENT_DESC desc{};
			desc.AlignedByteOffset = element.aligned_byte_offset;
			desc.Format = element.format;
			desc.InputSlot = element.input_slot;
			switch (element.input_slot_class)
			{
			case EInputClassification::PerVertexData:
				desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				break;
			case EInputClassification::PerInstanceData:
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