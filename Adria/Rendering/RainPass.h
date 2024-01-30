#pragma once
#include <memory>
#include "TextureHandle.h"
#include "Graphics/GfxDescriptor.h"
#include "Utilities/Delegate.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class RenderGraph;

	DECLARE_EVENT(RainEvent, RainPass, bool)

	class RainPass
	{
		static constexpr uint32 MAX_RAIN_DATA_BUFFER_SIZE = 1 << 20;

	public:
		RainPass(GfxDevice* gfx, uint32 w, uint32 h);

		void AddPass(RenderGraph& rg);
		void OnResize(uint32 w, uint32 h)
		{
			width = w, height = h;
		}
		void OnSceneInitialized();

		RainEvent& GetRainEvent() { return rain_event; }
		void OnRainEnabled(bool enabled)
		{
			rain_event.Broadcast(enabled && !cheap);
		}

		int32 GetRainSplashDiffuseIndex() const { return (int32)rain_splash_diffuse_handle; }
		int32 GetRainSplashBumpIndex()    const { return (int32)rain_splash_bump_handle;    }

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

		RainEvent rain_event;
	};
}