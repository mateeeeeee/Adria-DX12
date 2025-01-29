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
		GfxShaderCompilerFlag_None = 0,
		GfxShaderCompilerFlag_Debug = 1 << 0,
		GfxShaderCompilerFlag_DisableOptimization = 1 << 1
	};
	using GfxShaderCompilerFlags = Uint32;
	struct GfxShaderDesc
	{
		GfxShaderStage stage = GfxShaderStage::ShaderStageCount;
		GfxShaderModel model = SM_6_7;
		std::string file = "";
		std::string entry_point = "";
		std::vector<GfxShaderDefine> defines{};
		GfxShaderCompilerFlags flags = GfxShaderCompilerFlag_None;
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
		void SetShaderData(PCVoid data, Uint64 size)
		{
			shader_blob.resize(size);
			memcpy(shader_blob.data(), data, size);
		}

		GfxShaderDesc const& GetDesc() const { return desc; }

		PVoid GetData() const
		{
			return !shader_blob.empty() ? (PVoid)shader_blob.data() : nullptr;
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