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
	struct Shader
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
}