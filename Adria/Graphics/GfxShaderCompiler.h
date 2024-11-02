#pragma once
#include "GfxShader.h"
#include "Utilities/Delegate.h"

namespace adria
{
	struct GfxShaderCompileOutput
	{
		GfxShader shader;
		std::vector<std::string> includes;
		Uint64 shader_hash[2];
	};
	using GfxShaderCompileInput = GfxShaderDesc;

	namespace GfxShaderCompiler
	{
		void Initialize();
		void Destroy();
		bool CompileShader(GfxShaderCompileInput const& input, GfxShaderCompileOutput& output, bool bypass_cache);
		void ReadBlobFromFile(std::string const& filename, GfxShaderBlob& blob);
	}
}