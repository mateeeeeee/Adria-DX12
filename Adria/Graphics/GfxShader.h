#pragma once
#include <string>
#include <vector>
#include <d3d12.h>
#include "GfxShaderEnums.h"

namespace adria
{
	struct GfxShaderDefine
	{
		std::string name;
		std::string value;
	};
	
	enum GfxShaderCompilerFlagBit
	{
		ShaderCompilerFlag_None = 0,
		ShaderCompilerFlag_Debug = 1 << 0,
		ShaderCompilerFlag_DisableOptimization = 1 << 1
	};
	using GfxShaderCompilerFlags = Uint32;
	struct GfxShaderDesc
	{
		GfxShaderStage stage = GfxShaderStage::ShaderStageCount;
		GfxShaderModel model = SM_6_7;
		std::string file = "";
		std::string entry_point = "";
		std::vector<GfxShaderDefine> defines{};
		GfxShaderCompilerFlags flags = ShaderCompilerFlag_None;
	};

	using GfxShaderBlob = std::vector<Uint8>;
	using GfxDebugBlob = std::vector<Uint8>;
	class GfxShader
	{
	public:

		void SetDesc(GfxShaderDesc const& _desc)
		{
			desc = _desc;
		}
		void SetShaderData(void const* data, Uint64 size)
		{
			shader_blob.resize(size);
			memcpy(shader_blob.data(), data, size);
		}

		GfxShaderDesc const& GetDesc() const { return desc; }

		void* GetData() const
		{
			return !shader_blob.empty() ? (void*)shader_blob.data() : nullptr;
		}
		Uint64 GetSize() const
		{
			return shader_blob.size();
		}

		operator D3D12_SHADER_BYTECODE() const
		{
			return D3D12_SHADER_BYTECODE
			{
				.pShaderBytecode = GetData(),
				.BytecodeLength = GetSize()
			};
		}

	private:
		GfxShaderBlob shader_blob;
		GfxShaderDesc desc;
	};
}