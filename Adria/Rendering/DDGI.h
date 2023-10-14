#pragma once
#include "Graphics/GfxDescriptor.h"
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
			Vector3 start_position;
			int32 rays_per_probe;
			Vector3 probe_size;
			int32 max_rays_per_probe;
			Vector3i probe_count;
			float normal_bias;
			float energy_preservation;
			int32 irradiance_history_idx;
			int32 distance_history_idx;
		};

	public:

		DDGI(GfxDevice* gfx, entt::registry& reg, uint32 w, uint32 h);

		void OnSceneInitialized();
		void OnResize(uint32 w, uint32 h);
		void AddPasses(RenderGraph& rg);
		bool IsSupported() const { return is_supported; }

		int32 GetDDGIVolumeIndex();

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		uint32 width, height;
		bool is_supported;
		ArcPtr<ID3D12StateObject> ddgi_trace_so;

		DDGIVolume ddgi_volume;
		std::unique_ptr<GfxBuffer> ddgi_volume_buffer;
		GfxDescriptor ddgi_volume_buffer_srv;
		GfxDescriptor ddgi_volume_buffer_srv_gpu;

	private:

		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderID shader);
	};
}