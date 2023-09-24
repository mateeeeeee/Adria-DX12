#pragma once
#include "entt/entity/fwd.hpp"
 
namespace adria
{
	enum GfxShaderID : uint8;
	class GfxBuffer;
	class GfxTexture;
	class GfxDevice;
	class RenderGraph;

	class DDGI
	{
		static constexpr uint32 PROBE_IRRADIANCE_TEXELS = 6;
		static constexpr uint32 PROBE_DISTANCE_TEXELS = 14;
		static Vector2u ProbeTextureDimensions(Vector3u const& num_probes, uint32 texels_per_probe);

		struct DDGIVolume
		{
			Vector3				 origin;
			Vector3				 extents;
			Vector3u			 num_probes;
			uint32				 max_num_rays;
			uint32				 num_rays;
			std::unique_ptr<GfxTexture> irradiance_history;
			std::unique_ptr<GfxTexture> distance_history;
		};
		struct DDGIVolumeHLSL
		{
			Vector3 startPosition;
			int32 raysPerProbe;
			Vector3 probeSize;
			int32 maxRaysPerProbe;
			Vector3i probeCounts;
			float normalBias;
			float energyPreservation;
			int32 irradianceHistoryIdx;
			int32 distanceHistoryIdx;
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
		DDGIVolume ddgi_volume;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderID shader);
	};
}