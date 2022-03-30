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
		COUNT
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

			for (uint32_t i = 0; i < il_desc.size(); ++i)
				il_desc[i].SemanticName = semantic_names[i].c_str();

			desc.NumElements = static_cast<UINT>(il_desc.size());
			desc.pInputElementDescs = il_desc.data();

			return desc;
		}
	};

	struct ShaderDefine
	{
		std::wstring name;
		std::wstring value;
	};

	struct ShaderInfo
	{
		enum FLAGS
		{
			FLAG_NONE = 0,
			FLAG_DEBUG = 1 << 0,
			FLAG_DISABLE_OPTIMIZATION = 1 << 1,
		};

		EShaderStage stage = EShaderStage::COUNT;
		std::string shadersource = "";
		std::string entrypoint = "";
		std::vector<ShaderDefine> defines;
		UINT64 flags = FLAG_NONE;
	};

	namespace ShaderUtility
	{
		void Initialize();

		void Destroy();

		void CompileShader(ShaderInfo const& input, ShaderBlob& blob);

		void GetBlobFromCompiledShader(std::wstring_view filename, ShaderBlob& blob);

		void CreateInputLayoutWithReflection(ShaderBlob const& vs_blob,
			InputLayout& input_layout);
	}
}