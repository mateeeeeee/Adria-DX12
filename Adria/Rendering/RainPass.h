#pragma once
#include <memory>
#include "TextureHandle.h"
#include "RainBlockerMapPass.h"
#include "Graphics/GfxDescriptor.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class GfxGraphicsPipelineState;
	class GfxComputePipelineState;
	class RenderGraph;

	DECLARE_EVENT(RainEvent, RainPass, Bool)

	class RainPass
	{
		static constexpr Uint32 MAX_RAIN_DATA_BUFFER_SIZE = 1 << 20;

	public:
		RainPass(entt::registry& reg, GfxDevice* gfx, Uint32 w, Uint32 h);

		void Update(Float dt);
		void AddBlockerPass(RenderGraph& rg);
		void AddPass(RenderGraph& rg);
		void GUI();
		Bool IsEnabled() const;
		void OnResize(Uint32 w, Uint32 h)
		{
			width = w, height = h;
			rain_blocker_map_pass.OnResize(w, h);
		}
		void OnSceneInitialized();

		RainEvent& GetRainEvent() { return rain_event; }
		void OnRainEnabled(Bool enabled)
		{
			rain_event.Broadcast(enabled && !cheap);
		}

		Float GetRainTotalTime()		  const { return rain_total_time; }
		Int32 GetRainSplashDiffuseIndex() const { return (Int32)rain_splash_diffuse_handle; }
		Int32 GetRainSplashBumpIndex()    const { return (Int32)rain_splash_bump_handle;    }
		Int32 GetRainBlockerMapIndex()    const { return rain_blocker_map_pass.GetRainBlockerMapIdx(); }
		Matrix GetRainViewProjection()    const { return rain_blocker_map_pass.GetViewProjection(); }
	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxBuffer> rain_data_buffer;
		Uint32 width;
		Uint32 height;

		TextureHandle rain_streak_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle rain_splash_diffuse_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle rain_splash_bump_handle = INVALID_TEXTURE_HANDLE;
		Bool pause_simulation = false;
		Bool cheap = false;
		Float simulation_speed = 1.0f;
		Float rain_density = 0.5f;
		Float streak_scale = 0.33f;
		Float range_radius = 40.0f;
		Float rain_total_time = 0.0f;

		RainEvent rain_event;
		RainBlockerMapPass rain_blocker_map_pass;
		std::unique_ptr<GfxGraphicsPipelineState> rain_pso;
		std::unique_ptr<GfxComputePipelineState> rain_simulation_pso;

	private:
		void CreatePSOs();
	};
}