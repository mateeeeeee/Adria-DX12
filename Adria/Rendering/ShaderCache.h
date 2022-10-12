#pragma once
#include "Enums.h"
#include "../Events/Delegate.h"

namespace adria
{
	class GraphicsDevice;
	struct ShaderDesc;
	class Shader;

	DECLARE_MULTICAST_DELEGATE(ShaderRecompiledEvent, EShader);
	DECLARE_MULTICAST_DELEGATE(LibraryRecompiledEvent, EShader);

	class ShaderCache
	{
	public:
		static void Initialize();
		static void Destroy();
		static void CheckIfShadersHaveChanged();
		static ShaderRecompiledEvent& GetShaderRecompiledEvent();
		static LibraryRecompiledEvent& GetLibraryRecompiledEvent();

		[[deprecated("Use ShaderCache::CompileShader instead")]] 
		static Shader const& GetShader(EShader shader);

		//EShaderStage stage = EShaderStage::ShaderCount;
		//EShaderModel model;
		//std::string file = "";
		//std::string entry_point = "";
		//std::vector<ShaderMacro> macros;
		static Shader const& CompileShader(ShaderDesc const& desc);
	};
}