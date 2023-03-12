#pragma once
#include "Enums.h"
#include "../Events/Delegate.h"

namespace adria
{
	class GfxDevice;
	class GfxShader;

	DECLARE_MULTICAST_DELEGATE(ShaderRecompiledEvent, GfxShaderID);
	DECLARE_MULTICAST_DELEGATE(LibraryRecompiledEvent, GfxShaderID);

	class ShaderCache
	{
	public:
		static void Initialize();
		static void Destroy();
		static void CheckIfShadersHaveChanged();
		static ShaderRecompiledEvent& GetShaderRecompiledEvent();
		static LibraryRecompiledEvent& GetLibraryRecompiledEvent();
		static GfxShader const& GetShader(GfxShaderID shader);
	};
}