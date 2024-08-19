#pragma once
#include "PostEffect.h"
#include "BlurPass.h"
#include "HelperPasses.h"
#include "Graphics/GfxRayTracingShaderTable.h"

namespace adria
{
	class RenderGraph;
	class GfxDevice;
	class GfxStateObject;
	class GfxShaderKey;

	class RayTracedReflectionsPass : public PostEffect
	{
	public:
		RayTracedReflectionsPass(GfxDevice* gfx, uint32 width, uint32 height);
		~RayTracedReflectionsPass();

		virtual void AddPass(RenderGraph&, PostProcessor*) override;
		virtual void OnResize(uint32 w, uint32 h) override;
		virtual bool IsEnabled(PostProcessor const*) const override;
		virtual void GUI() override;
		virtual bool IsSupported() const override;

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxStateObject> ray_traced_reflections_so;
		uint32 width, height;
		BlurPass blur_pass;

		bool is_supported;
		float reflection_roughness_scale = 0.0f;
		CopyToTexturePass copy_to_texture_pass;

	private:
		void CreateStateObject();
		void OnLibraryRecompiled(GfxShaderKey const&);
	};
}