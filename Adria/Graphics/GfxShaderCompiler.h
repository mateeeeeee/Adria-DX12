#pragma once
#include "GfxShader.h"
#include "GfxInputLayout.h"

namespace adria
{
	struct GfxShaderCompileOutput
	{
		GfxShader shader;
		std::vector<std::string> dependent_files;
	};

	namespace GfxShaderCompiler
	{
		void Initialize();
		void Destroy();
		bool CompileShader(GfxShaderDesc const& desc, GfxShaderCompileOutput& output);
		void ReadBlobFromFile(std::wstring_view filename, GfxShaderBlob& blob);
		void CreateInputLayout(GfxShader const& vertex_shader, GfxInputLayout& input_layout);
	}
}