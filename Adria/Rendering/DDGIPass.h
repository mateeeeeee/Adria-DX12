#pragma once
#include "Graphics/GfxDescriptor.h"
#include "entt/entity/fwd.hpp"

namespace adria
{
	class GfxBuffer;
	class GfxTexture;
	class GfxDevice;
	class GfxShaderKey;
	class GfxStateObject;
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;
	class RenderGraph;

	class DDGIPass
	{
		static constexpr Uint32 PROBE_IRRADIANCE_TEXELS = 6;
		static constexpr Uint32 PROBE_DISTANCE_TEXELS = 14;
		static Vector2u ProbeTextureDimensions(Vector3u const& num_probes, Uint32 texels_per_probe);

		struct DDGIVolume
		{
			Vector3				 origin;
			Vector3				 extents;
			Vector3u			 num_probes;
			Uint32				 max_num_rays;
			Uint32				 num_rays;
			std::unique_ptr<GfxTexture> irradiance_history;
			std::unique_ptr<GfxTexture> distance_history;
			GfxDescriptor irradiance_history_srv;
			GfxDescriptor distance_history_srv;
		};
		struct DDGIVolumeGPU
		{
			Vector3 start_position;
			Int32 rays_per_probe;
			Vector3 probe_size;
			Int32 max_rays_per_probe;
			Vector3i probe_count;
			Float normal_bias;
			Float energy_preservation;
			Int32 irradiance_history_idx;
			Int32 distance_history_idx;
		};

		enum DDGIVisualizeMode : Uint32
		{
			DDGIVisualizeMode_Irradiance = 0,
			DDGIVisualizeMode_Distance
		};
	public:

		DDGIPass(GfxDevice* gfx, entt::registry& reg, Uint32 w, Uint32 h);
		~DDGIPass();

		void OnSceneInitialized();
		void OnResize(Uint32 w, Uint32 h);

		void AddPasses(RenderGraph& rg);
		void AddVisualizePass(RenderGraph& rg);
		void GUI();

		Bool Visualize() const   { return visualize; }
		Bool IsEnabled() const;
		Bool IsSupported() const { return is_supported; }
		Int32 GetDDGIVolumeIndex();

	private:
		GfxDevice* gfx;
		entt::registry& reg;
		Uint32 width, height;
		Bool is_supported;
		std::unique_ptr<GfxStateObject> ddgi_trace_so;
		DDGIVolume ddgi_volume;
		std::unique_ptr<GfxBuffer>  ddgi_volume_buffer;
		GfxDescriptor ddgi_volume_buffer_srv;
		Bool visualize = false;
		DDGIVisualizeMode ddgi_visualize_mode = DDGIVisualizeMode_Irradiance;
		std::unique_ptr<GfxComputePipelineState>  update_irradiance_pso;
		std::unique_ptr<GfxComputePipelineState>  update_distance_pso;
		std::unique_ptr<GfxGraphicsPipelineState> visualize_probes_pso;

	private:
		void CreatePSOs();
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}