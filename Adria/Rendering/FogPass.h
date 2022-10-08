#pragma once
#include "Enums.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	struct FogParameters
	{
		EFogType fog_type = EFogType::Exponential;
		float32  fog_falloff = 0.005f;
		float32  fog_density = 0.002f;
		float32  fog_start = 100.0f;
		float32  fog_color[3] = { 0.5f,0.6f,0.7f };
	};

	class FogPass
	{
	public:
		FogPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

		FogParameters const& GetParams() const { return params; }

	private:
		uint32 width, height;
		FogParameters params{};
	};

}