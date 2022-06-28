#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include "../Core/Macros.h"

namespace adria
{
	enum class EShaderStage
	{
		VS,
		PS,
		HS,
		DS,
		GS,
		CS,
		LIB,
		ShaderCount
	};

	struct ShaderBlob
	{
		std::vector<uint8_t> bytecode;

		void* GetPointer() const
		{
			return (void*)bytecode.data();
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

	struct InputLayout
	{
		mutable std::vector<D3D12_INPUT_ELEMENT_DESC> il_desc;
		std::vector<std::string> semantic_names;
		operator D3D12_INPUT_LAYOUT_DESC() const
		{
			D3D12_INPUT_LAYOUT_DESC desc{};
			ADRIA_ASSERT(semantic_names.size() == il_desc.size());
			for (uint32_t i = 0; i < il_desc.size(); ++i) il_desc[i].SemanticName = semantic_names[i].c_str();
			desc.NumElements = static_cast<UINT>(il_desc.size());
			desc.pInputElementDescs = desc.NumElements ? il_desc.data() : nullptr;
			return desc;
		}
	};

	struct ShaderMacro
	{
		std::wstring name;
		std::wstring value;
	};
}