#pragma once
#include "../Graphics/Texture.h"
#include "../Graphics/Buffer.h"

namespace adria
{
	struct RenderGraphResourceHandle
	{
		inline constexpr static size_t invalid_id = size_t(-1);

		RenderGraphResourceHandle() : id(invalid_id) {}
		RenderGraphResourceHandle(size_t _id) : id(_id) {}

		void Invalidate() { id = invalid_id; }
		bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceHandle const&) const = default;

		size_t id;
	};

	template<typename ResourceType>
	struct TypedRenderGraphResourceHandle : RenderGraphResourceHandle {};

	using RGBufferHandle = TypedRenderGraphResourceHandle<Buffer>;
	using RGTextureHandle = TypedRenderGraphResourceHandle<Texture>;
}

namespace std 
{
	template <> struct hash<adria::RGTextureHandle>
	{
		size_t operator()(adria::RGTextureHandle const& h) const
		{
			return hash<size_t>()(h.id);
		}
	};
	template <> struct hash<adria::RGBufferHandle>
	{
		size_t operator()(adria::RGBufferHandle const& h) const
		{
			return hash<size_t>()(h.id);
		}
	};
}