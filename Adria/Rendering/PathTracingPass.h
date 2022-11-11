#pragma once
#include "Enums.h"
#include "../Graphics/RayTracingUtil.h"
#include "../Core/Definitions.h"
#include "../RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class Texture;
	class GraphicsDevice;

	class PathTracingPass
	{
	public:
		PathTracingPass(GraphicsDevice* gfx, uint32 width, uint32 height);
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		bool IsSupported() const;
		void Reset();
	private:
		GraphicsDevice* gfx;
		Microsoft::WRL::ComPtr<ID3D12StateObject> path_tracing;
		uint32 width, height;
		bool is_supported;
		std::unique_ptr<Texture> accumulation_texture = nullptr;
		int32 accumulated_frames = 1;
		int32 max_bounces = 3;
	private:
		void CreateStateObject();
		void OnLibraryRecompiled(EShaderId shader);
	};
}