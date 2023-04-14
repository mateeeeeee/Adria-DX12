#pragma once
#include "Enums.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"


namespace adria
{
	class RenderGraph;

	class FogPass
	{

		enum class FogType : int32
		{
			Exponential,
			ExponentialHeight
		};

		struct FogParameters
		{
			FogType fog_type = FogType::Exponential;
			float  fog_falloff = 0.005f;
			float  fog_density = 0.002f;
			float  fog_start = 100.0f;
			float  fog_color[3] = { 0.5f,0.6f,0.7f };
		};

	public:
		FogPass(uint32 w, uint32 h);

		RGResourceName AddPass(RenderGraph& rendergraph, RGResourceName input);
		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
		FogParameters params{};
	};

}