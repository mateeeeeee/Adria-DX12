#pragma once
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class RenderGraph;

	struct VelocityBufferParams
	{
		float32 velocity_buffer_scale = 64.0f;
	};

	class VelocityBufferPass
	{
	public:
		VelocityBufferPass(uint32 w, uint32 h);

		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);

		VelocityBufferParams const& GetParams() const { return params; }

	private:
		uint32 width, height;
		VelocityBufferParams params{};
	};
}