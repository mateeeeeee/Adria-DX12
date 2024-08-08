#pragma once
#include "RenderGraph/RenderGraphResourceName.h"

namespace adria
{
	class RenderGraph;
	class PostProcessor;

	class PostEffect
	{
	public:
		virtual ~PostEffect() = default;

		virtual void AddPass(RenderGraph&, PostProcessor*) = 0;
		virtual void OnResize(uint32, uint32) = 0;
		virtual bool IsEnabled(PostProcessor*) const = 0;
		virtual void SetEnabled(bool) {}
		virtual void OnSceneInitialized() {}
		virtual void GUI() {}
		virtual bool IsSupported() const { return true; }
	};

	class EmptyPostEffect : public PostEffect
	{
	public:
		EmptyPostEffect() {}

		virtual void AddPass(RenderGraph&, PostProcessor*) {}
		virtual void OnResize(uint32, uint32) {}
		virtual bool IsEnabled(PostProcessor*) const { return true; }
	};

	class PostEffectGroup : public PostEffect
	{
	public:

		virtual void AddPass(RenderGraph& rg, PostProcessor* postprocessor) override
		{
			ADRIA_ASSERT(post_effect_idx < post_effects.size());
			post_effects[post_effect_idx]->AddPass(rg, postprocessor);
		}
		virtual void OnResize(uint32 w, uint32 h) override
		{
			for (auto& post_effect : post_effects) post_effect->OnResize(w, h);
		}
		virtual bool IsEnabled(PostProcessor* postprocessor) const override
		{
			ADRIA_ASSERT(post_effect_idx < post_effects.size());
			return post_effects[post_effect_idx]->IsEnabled(postprocessor);
		}
		virtual void OnSceneInitialized() override
		{
			for (auto& post_effect : post_effects) post_effect->OnSceneInitialized();
		}
		virtual void GUI() override
		{
			GroupGUI();
			post_effects[post_effect_idx]->GUI();
		}

	protected:
		std::vector<std::unique_ptr<PostEffect>> post_effects;
		uint32 post_effect_idx;

	protected:

		virtual void GroupGUI() {}
	};
}