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
		virtual bool IsEnabled(PostProcessor*) const = 0;
		virtual void AddPass(RenderGraph& rg, PostProcessor*) = 0;
		virtual void OnResize(uint32 w, uint32 h) = 0;
		virtual void OnSceneInitialized() {}
	};

	class PostEffectGroup final : public PostEffect
	{
	public:
		~PostEffectGroup()
		{
			for (PostEffect* post_effect : post_effects) delete post_effect;
			post_effects.clear();
		}

		virtual bool IsEnabled(PostProcessor* postprocessor) const override
		{
			ADRIA_ASSERT(post_effect_idx < post_effects.size());
			return post_effects[post_effect_idx]->IsEnabled(postprocessor);
		}

		virtual RGResourceName AddPass(RenderGraph& rg, RGResourceName source) override
		{
			ADRIA_ASSERT(post_effect_idx < post_effects.size());
			return post_effects[post_effect_idx]->AddPass(rg, source);
		}
		virtual void OnResize(uint32 w, uint32 h) override
		{
			for (PostEffect* post_effect : post_effects) post_effect->OnResize(w, h);
		}
		virtual void OnSceneInitialized() override
		{
			for (PostEffect* post_effect : post_effects) post_effect->OnSceneInitialized();
		}

	protected:
		std::vector<PostEffect*> post_effects;
		uint32 post_effect_idx;
	};
}