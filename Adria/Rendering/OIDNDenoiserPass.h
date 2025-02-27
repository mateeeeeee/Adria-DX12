#pragma once
#include "OpenImageDenoise/oidn.h"
#include "Graphics/GfxFence.h"
#include "IDenoiserPass.h"

namespace adria
{
	class GfxDevice;
	class GfxBuffer;
	class GfxTexture;
	class GfxCommandList;

	class OIDNDenoiserPass : public IDenoiserPass
	{
	public:
		explicit OIDNDenoiserPass(GfxDevice* gfx);
		~OIDNDenoiserPass();

		virtual Bool IsSupported() const override { return supported; }
		virtual void AddPass(RenderGraph& rendergraph) override;
		virtual void Reset() override;
		virtual DenoiserType GetType() const override { return DenoiserType_OIDN; }

	private:
		GfxDevice* gfx;
		OIDNDevice oidn_device = nullptr;
		OIDNFilter oidn_filter = nullptr;
		OIDNBuffer oidn_color_buffer = nullptr;
		OIDNBuffer oidn_albedo_buffer = nullptr;
		OIDNBuffer oidn_normal_buffer = nullptr;
		GfxFence oidn_fence;
		Uint64 oidn_fence_value = 0;

		std::unique_ptr<GfxBuffer> color_buffer;
		std::unique_ptr<GfxBuffer> albedo_buffer;
		std::unique_ptr<GfxBuffer> normal_buffer;
		Bool denoised = false;
		Bool supported = false;

	private:
		void CreateBuffers(GfxTexture& color_texture, GfxTexture const& albedo_texture, GfxTexture const& normal_texture);
		void ReleaseBuffers();

		void Denoise(GfxCommandList* cmd_list, GfxTexture& color_texture, GfxTexture const& albedo_texture, GfxTexture const& normal_texture);
	};
}