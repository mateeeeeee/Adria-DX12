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
	enum EShaderModel
	{
		SM_6_0,
		SM_6_1,
		SM_6_2,
		SM_6_3,
		SM_6_4,
		SM_6_5,
		SM_6_6
	};

	enum EShaderCompilerFlagBit
	{
		ShaderCompilerFlag_None = 0,
		ShaderCompilerFlag_Debug = 1 << 0,
		ShaderCompilerFlag_DisableOptimization = 1 << 1
	};
	using ShaderCompilerFlags = uint32;
	struct ShaderDesc
	{
		EShaderStage stage = EShaderStage::ShaderCount;
		EShaderModel model = SM_6_6;
		std::string file = "";
		std::string entry_point = "";
		std::vector<ShaderMacro> macros{};
		ShaderCompilerFlags flags = ShaderCompilerFlag_None;
	};

	using ShaderBlob = std::vector<uint8>;
	class Shader
	{
	public:

		void SetDesc(ShaderDesc const& _desc)
		{
			desc = _desc;
		}
		void SetBytecode(void const* data, size_t size)
		{
			bytecode.resize(size);
			memcpy(bytecode.data(), data, size);
		}

		ShaderDesc const& GetDesc() const { return desc; }
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

	private:
		ShaderBlob bytecode;
		ShaderDesc desc;
	};
}