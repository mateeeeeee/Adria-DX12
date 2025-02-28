#pragma once
#include "IDenoiserPass.h"
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
		void GUI();

		RGResourceName GetFinalOutput() const;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> path_tracing_so;
		std::unique_ptr<GfxStateObject> path_tracing_so_write_gbuffer;
		Uint32 width, height;
		Bool is_supported;
		std::unique_ptr<GfxTexture> accumulation_texture = nullptr;
		std::unique_ptr<GfxTexture> denoiser_albedo_texture = nullptr;
		std::unique_ptr<GfxTexture> denoiser_normal_texture = nullptr;
		Int32 accumulated_frames = 1;
		Int32 max_bounces = 3;
		std::unique_ptr<IDenoiserPass> denoiser_pass;

	private:
		void CreateStateObject();
		GfxStateObject* CreateStateObjectCommon(GfxShaderKey const&);
		void OnLibraryRecompiled(GfxShaderKey const&);

		void CreateAccumulationTexture();
		void CreateDenoiserTextures();
	};
}