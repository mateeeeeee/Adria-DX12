#pragma once
#include "Enums.h"
#include "../Events/Delegate.h"

namespace adria
{
	class GraphicsDevice;
	class Shader;

	DECLARE_MULTICAST_DELEGATE(ShaderRecompiledEvent, EShaderId);
	DECLARE_MULTICAST_DELEGATE(LibraryRecompiledEvent, EShaderId);

	class ShaderCache
	{
	public:
		static void Initialize();
		static void Destroy();
		static void CheckIfShadersHaveChanged();
		static ShaderRecompiledEvent& GetShaderRecompiledEvent();
		static LibraryRecompiledEvent& GetLibraryRecompiledEvent();
		static Shader const& GetShader(EShaderId shader);
	};
}