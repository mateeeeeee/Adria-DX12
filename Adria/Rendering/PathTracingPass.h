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
		PathTracingPass(GfxDevice* gfx, Uint32 width, Uint32 height);
		~PathTracingPass();
		void AddPass(RenderGraph& rendergraph);
		void OnResize(Uint32 w, Uint32 h);
		Bool IsSupported() const;
		void Reset();

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> path_tracing_so;
		Uint32 width, height;
		Bool is_supported;
		std::unique_ptr<GfxTexture> accumulation_texture = nullptr;
		Sint32 accumulated_frames = 1;
		Sint32 max_bounces = 3;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}