#pragma once
#include "../Graphics/Texture.h"
#include "../Graphics/Buffer.h"

namespace adria
{
	enum class ERGResourceType : uint8
	{
		Buffer,
		Texture
	};

	enum class ERGResourceViewType : uint8
	{
		SRV,
		UAV,
		RTV,
		DSV
	};

	struct RenderGraphResourceRef
	{
		inline constexpr static uint32 invalid_id = uint32(-1);

		RenderGraphResourceRef() : id(invalid_id) {}
		RenderGraphResourceRef(RenderGraphResourceRef const&) = default;
		explicit RenderGraphResourceRef(size_t _id) : id(static_cast<uint32>(_id)) {}

		void Invalidate() { id = invalid_id; }
		bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceRef const&) const = default;

		uint32 id;
	};

	template<ERGResourceType ResourceType>
	struct TypedRenderGraphResourceHandle : RenderGraphResourceRef 
	{
		using RenderGraphResourceRef::RenderGraphResourceRef;
	};

	using RGBufferRef = TypedRenderGraphResourceHandle<ERGResourceType::Buffer>;
	using RGTextureRef = TypedRenderGraphResourceHandle<ERGResourceType::Texture>;

	struct RenderGraphResourceViewRef
	{
		inline constexpr static uint64 invalid_id = uint64(-1);

		RenderGraphResourceViewRef() : id(invalid_id) {}
		RenderGraphResourceViewRef(size_t view_id, RenderGraphResourceRef resource_handle)
			: id(invalid_id)
		{
			uint32 _resource_id = resource_handle.id;
			id = (view_id << 32) | _resource_id;
		}

		size_t GetViewId() const { return (id >> 32); };
		size_t GetResourceId() const
		{
			return (size_t)static_cast<uint32>(id);
		};

		void Invalidate() { id = invalid_id; }
		bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceViewRef const&) const = default;

		uint64 id;
	};

	template<ERGResourceType ResourceType, ERGResourceViewType ResourceViewType>
	struct TypedRenderGraphResourceViewRef : RenderGraphResourceViewRef 
	{
		using RenderGraphResourceViewRef::RenderGraphResourceViewRef;

		auto GetResourceHandle() const
		{
			if constexpr (ResourceType == ERGResourceType::Buffer) return RGBufferRef(GetResourceId());
			else if constexpr (ResourceType == ERGResourceType::Texture) return RGTextureRef(GetResourceId());
		}
	};

	using RGTextureSRVRef = TypedRenderGraphResourceViewRef<ERGResourceType::Texture,  ERGResourceViewType::SRV>;
	using RGTextureUAVRef = TypedRenderGraphResourceViewRef<ERGResourceType::Texture,  ERGResourceViewType::UAV>;
	using RGTextureRTVRef = TypedRenderGraphResourceViewRef<ERGResourceType::Texture,  ERGResourceViewType::RTV>;
	using RGTextureDSVRef = TypedRenderGraphResourceViewRef<ERGResourceType::Texture,  ERGResourceViewType::DSV>;
	using RGBufferSRVRef  = TypedRenderGraphResourceViewRef<ERGResourceType::Buffer,   ERGResourceViewType::SRV>;
	using RGBufferUAVRef  = TypedRenderGraphResourceViewRef<ERGResourceType::Buffer,   ERGResourceViewType::UAV>;
}

namespace std 
{
	template <> struct hash<adria::RGTextureRef>
	{
		size_t operator()(adria::RGTextureRef const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGBufferRef>
	{
		size_t operator()(adria::RGBufferRef const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureSRVRef>
	{
		size_t operator()(adria::RGTextureSRVRef const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureUAVRef>
	{
		size_t operator()(adria::RGTextureUAVRef const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureRTVRef>
	{
		size_t operator()(adria::RGTextureRTVRef const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureDSVRef>
	{
		size_t operator()(adria::RGTextureDSVRef const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
}