#pragma once

#include "Graphics/GfxRayTracingShaderTable.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class GfxTexture;
	class GfxDevice;
	class GfxShaderKey;
	class GfxStateObject;

	class PathTracingPass
	{
	public:
		PathTracingPass(GfxDevice* gfx, uint32 width, uint32 height);
		~PathTracingPass();
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		bool IsSupported() const;
		void Reset();

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> path_tracing_so;
		uint32 width, height;
		bool is_supported;
		std::unique_ptr<GfxTexture> accumulation_texture = nullptr;
		int32 accumulated_frames = 1;
		int32 max_bounces = 3;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}