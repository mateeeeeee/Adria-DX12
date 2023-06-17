#pragma once
#include <optional>
#include "CopyToTexturePass.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceId.h"

namespace adria
{
	class RenderGraph;
	class VolumetricLightingPass
	{
		enum VolumetricLightingResolution
		{
			VolumetricLightingResolution_Full = 0,
			VolumetricLightingResolution_Half = 1,
			VolumetricLightingResolution_Quarter = 2
		};

	public:
		VolumetricLightingPass(uint32 w, uint32 h) : width(w), height(h), copy_to_texture_pass(w,h) {}
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			copy_to_texture_pass.OnResize(w, h);
		}

	private:
		CopyToTexturePass copy_to_texture_pass;
		uint32 width, height;
		VolumetricLightingResolution resolution = VolumetricLightingResolution_Full;
	};

}