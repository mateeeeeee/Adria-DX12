#pragma once
#include "Enums.h"

namespace adria
{
	class GraphicsDevice;
	struct ShaderBlob;

	class ShaderManager
	{
	public:
		static void Initialize();
		static void Destroy();
		static void CheckIfShadersHaveChanged();
		static ShaderBlob const& GetShader(EShader shader);
	};
}