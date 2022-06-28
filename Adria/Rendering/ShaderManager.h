#pragma once
#include "Enums.h"

namespace adria
{
	class GraphicsDevice;
	struct ShaderBlob;

	class ShaderManager
	{
		friend class PSOManager;
	public:
		static void Initialize();
		static void Destroy();
		static void CheckIfShadersHaveChanged();
	private:
		static ShaderBlob const& GetShader(EShader shader);
	};
}