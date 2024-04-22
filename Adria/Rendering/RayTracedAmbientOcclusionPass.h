#pragma once

#include "BlurPass.h"
#include "Graphics/GfxRayTracingShaderTable.h"
#include "Core/CoreTypes.h"
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	enum GfxShaderID : uint8;
	class RenderGraph;
	class GfxDevice;

	class RayTracedAmbientOcclusionPass
	{
		struct RTAOParams
		{
			float radius = 2.0f;
			float power_log = -1.0f;
			float filter_distance_sigma = 10.0f;
			float filter_depth_sigma = 0.25f;
		};

	public:
		RayTracedAmbientOcclusionPass(GfxDevice* gfx, uint32 width, uint32 height);
		void AddPass(RenderGraph& rendergraph);
		void OnResize(uint32 w, uint32 h);
		bool IsSupported() const;

	private:
		GfxDevice* gfx;
		BlurPass blur_pass;
		Handle<ID3D12StateObject> ray_traced_ambient_occlusion;
		uint32 width, height;
		bool is_supported;
		RTAOParams params{};

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderID shader);
	};
}