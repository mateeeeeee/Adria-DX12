#pragma once
#include "Shader.h"
#include "InputLayout.h"

namespace adria
{
	struct ShaderCompileOutput
	{
		Shader shader;
		std::vector<std::string> dependent_files;
	};

	namespace ShaderCompiler
	{
		void Initialize();
		void Destroy();
		bool CompileShader(ShaderDesc const& desc, ShaderCompileOutput& output);
		void ReadBlobFromFile(std::wstring_view filename, ShaderBlob& blob);
		void CreateInputLayout(Shader const& vertex_shader, InputLayout& input_layout);
	}
}