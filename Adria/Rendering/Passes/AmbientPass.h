#pragma once
#include "../../Core/Definitions.h"
#include "../../RenderGraph/RenderGraphResourceRef.h"
#include "../../RenderGraph/RenderGraphBlackboard.h"


namespace adria
{
	namespace tecs
	{
		class registry;
	}
	class RenderGraph;
	class DescriptorHeap;

	struct AmbientPassData
	{
		RGTextureRef hdr_rt;
		RGTextureSRVRef gbuffer_normal_srv;
		RGTextureSRVRef gbuffer_albedo_srv;
		RGTextureSRVRef gbuffer_emissive_srv;
		RGTextureSRVRef depth_stencil_srv;
	};

	class AmbientPass
	{
	public:
		AmbientPass(uint32 w, uint32 h);

		AmbientPassData const& AddPass(RenderGraph& rendergraph, RGBlackboard& blackboard,
			RGTextureRef gbuffer_normal,
			RGTextureRef gbuffer_albedo,
			RGTextureRef gbuffer_emissive,
			RGTextureRef depth_stencil, std::optional<RGTextureSRVRef> ambient_occlusion_texture = std::nullopt);

		void OnResize(uint32 w, uint32 h);

	private:
		uint32 width, height;
		std::unique_ptr<DescriptorHeap> null_srv_heap;
	};

}