#pragma once
#include "Shader.h"
#include "InputLayout.h"

namespace adria
{
	struct ShaderCompileInput
	{
		enum EShaderCompileFlags
		{
			FlagNone = 0,
			FlagDebug = 1 << 0,
			FlagDisableOptimization = 1 << 1,
		};

		EShaderStage stage = EShaderStage::ShaderCount;
		std::string source_file = "";
		std::string entrypoint = "";
		std::vector<ShaderMacro> macros;
		UINT64 flags = FlagNone;
	};

	struct ShaderCompileOutput
	{
		Shader blob;
		std::vector<std::string> dependent_files;
	};

	namespace ShaderCompiler
	{
		void Initialize();
		void Destroy();
		void CompileShader(ShaderCompileInput const& input, ShaderCompileOutput& blob);
		void GetBlobFromCompiledShader(std::wstring_view filename, Shader& blob);
		void CreateInputLayout(Shader const& vs_blob, InputLayout& input_layout);
	}
}