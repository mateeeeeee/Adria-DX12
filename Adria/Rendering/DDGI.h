#pragma once
#include <DirectXCollision.h>
#include "Enums.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxBuffer;
	class GfxTexture;
	class GfxDevice;
	class RenderGraph;

	class DDGI
	{
		static constexpr uint32 PROBE_IRRADIANCE_TEXELS = 6;
		static constexpr uint32 PROBE_DEPTH_TEXELS = 14;
		static DirectX::XMUINT2 ProbeTextureDimensions(DirectX::XMUINT3 const& num_probes, uint32 texels_per_probe);

		struct DDGIVolume
		{
			DirectX::BoundingBox bounding_box;
			DirectX::XMUINT3	 num_probes;
			uint32				 max_num_rays;
			uint32				 num_rays;
			std::unique_ptr<GfxTexture> irradiance_history;
			std::unique_ptr<GfxTexture> depth_history;
		};

	public:

		DDGI(GfxDevice* gfx, entt::registry& reg, uint32 w, uint32 h);

		void OnResize(uint32 w, uint32 h);
		void AddPasses(RenderGraph& rg);
		bool IsSupported() const { return is_supported; }

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		uint32 width, height;
		bool is_supported;
		ArcPtr<ID3D12StateObject> ddgi_trace_so;
		std::vector<DDGIVolume> ddgi_volumes;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderID shader);
	};
}