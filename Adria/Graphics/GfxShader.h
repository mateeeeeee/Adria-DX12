#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include "../Core/Defines.h"
#include "../Core/CoreTypes.h"

namespace adria
{
	struct GfxShaderMacro
	{
		std::wstring name;
		std::wstring value;
	};
	enum class GfxShaderStage
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
	enum GfxShaderModel
	{
		SM_6_0,
		SM_6_1,
		SM_6_2,
		SM_6_3,
		SM_6_4,
		SM_6_5,
		SM_6_6
	};

	enum GfxShaderCompilerFlagBit
	{
		ShaderCompilerFlag_None = 0,
		ShaderCompilerFlag_Debug = 1 << 0,
		ShaderCompilerFlag_DisableOptimization = 1 << 1
	};
	using GfxShaderCompilerFlags = uint32;
	struct GfxShaderDesc
	{
		GfxShaderStage stage = GfxShaderStage::ShaderCount;
		GfxShaderModel model = SM_6_6;
		std::string file = "";
		std::string entry_point = "";
		std::vector<GfxShaderMacro> macros{};
		GfxShaderCompilerFlags flags = ShaderCompilerFlag_None;
	};

	using GfxShaderBlob = std::vector<uint8>;
	class GfxShader
	{
	public:

		void SetDesc(GfxShaderDesc const& _desc)
		{
			desc = _desc;
		}
		void SetBytecode(void const* data, size_t size)
		{
			bytecode.resize(size);
			memcpy(bytecode.data(), data, size);
		}

		GfxShaderDesc const& GetDesc() const { return desc; }
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
		GfxShaderBlob bytecode;
		GfxShaderDesc desc;
	};
}