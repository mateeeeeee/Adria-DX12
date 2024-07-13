#pragma once

namespace adria
{
	class GfxShader;
	struct GfxInputLayout;

	namespace GfxReflection
	{
		void FillInputLayoutDesc(GfxShader const& vertex_shader, GfxInputLayout& input_layout);
	}
}