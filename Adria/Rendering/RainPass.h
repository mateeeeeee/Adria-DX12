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

	DECLARE_EVENT(RainEvent, RainPass, bool)

	class RainPass
	{
		static constexpr uint32 MAX_RAIN_DATA_BUFFER_SIZE = 1 << 20;

	public:
		RainPass(entt::registry& reg, GfxDevice* gfx, uint32 w, uint32 h);

		void Update(float dt);
		void AddBlockerPass(RenderGraph& rg);
		void AddPass(RenderGraph& rg);
		void GUI();
		bool IsEnabled() const;
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
			rain_blocker_map_pass.OnResize(w, h);
		}
		void OnSceneInitialized();

		RainEvent& GetRainEvent() { return rain_event; }
		void OnRainEnabled(bool enabled)
		{
			rain_event.Broadcast(enabled && !cheap);
		}

		float GetRainTotalTime()		  const { return rain_total_time; }
		int32 GetRainSplashDiffuseIndex() const { return (int32)rain_splash_diffuse_handle; }
		int32 GetRainSplashBumpIndex()    const { return (int32)rain_splash_bump_handle;    }
		int32 GetRainBlockerMapIndex()    const { return rain_blocker_map_pass.GetRainBlockerMapIdx(); }
		Matrix GetRainViewProjection()    const { return rain_blocker_map_pass.GetViewProjection(); }
	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxBuffer> rain_data_buffer;
		uint32 width;
		uint32 height;

		TextureHandle rain_streak_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle rain_splash_diffuse_handle = INVALID_TEXTURE_HANDLE;
		TextureHandle rain_splash_bump_handle = INVALID_TEXTURE_HANDLE;
		bool pause_simulation = false;
		bool cheap = false;
		float simulation_speed = 1.0f;
		float rain_density = 0.5f;
		float streak_scale = 0.33f;
		float range_radius = 40.0f;
		float rain_total_time = 0.0f;

		RainEvent rain_event;
		RainBlockerMapPass rain_blocker_map_pass;
		std::unique_ptr<GfxGraphicsPipelineState> rain_pso;
		std::unique_ptr<GfxComputePipelineState> rain_simulation_pso;

	private:
		void CreatePSOs();
	};
}