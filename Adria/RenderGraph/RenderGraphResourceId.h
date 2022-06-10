#pragma once
#include "../Core/Definitions.h"

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

	struct RenderGraphResourceId
	{
		inline constexpr static uint32 invalid_id = uint32(-1);

		RenderGraphResourceId() : id(invalid_id) {}
		RenderGraphResourceId(RenderGraphResourceId const&) = default;
		explicit RenderGraphResourceId(size_t _id) : id(static_cast<uint32>(_id)) {}

		void Invalidate() { id = invalid_id; }
		bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceId const&) const = default;

		uint32 id;
	};
	using RGResourceId = RenderGraphResourceId;

	template<ERGResourceType ResourceType>
	struct TypedRenderGraphResourceHandle : RGResourceId
	{
		using RGResourceId::RGResourceId;
	};

	using RGBufferId = TypedRenderGraphResourceHandle<ERGResourceType::Buffer>;
	using RGTextureId = TypedRenderGraphResourceHandle<ERGResourceType::Texture>;

	struct RenderGraphResourceViewRef
	{
		inline constexpr static uint64 invalid_id = uint64(-1);

		RenderGraphResourceViewRef() : id(invalid_id) {}
		RenderGraphResourceViewRef(size_t view_id, RenderGraphResourceId resource_handle)
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
			if constexpr (ResourceType == ERGResourceType::Buffer) return RGBufferId(GetResourceId());
			else if constexpr (ResourceType == ERGResourceType::Texture) return RGTextureId(GetResourceId());
		}
	};

	using RGRenderTargetId = TypedRenderGraphResourceViewRef<ERGResourceType::Texture, ERGResourceViewType::RTV>;
	using RGDepthStencilId = TypedRenderGraphResourceViewRef<ERGResourceType::Texture, ERGResourceViewType::DSV>;
	using RGTextureReadOnlyId	= TypedRenderGraphResourceViewRef<ERGResourceType::Texture,  ERGResourceViewType::SRV>;
	using RGTextureReadWriteId	= TypedRenderGraphResourceViewRef<ERGResourceType::Texture,  ERGResourceViewType::UAV>;
	using RGTextureCopySrcId	= RGTextureId;
	using RGTextureCopyDstId	= RGTextureId;

	using RGBufferReadOnlyId	= TypedRenderGraphResourceViewRef<ERGResourceType::Buffer,   ERGResourceViewType::SRV>;
	using RGBufferReadWriteId	= TypedRenderGraphResourceViewRef<ERGResourceType::Buffer,   ERGResourceViewType::UAV>;
	using RGBufferCopySrcId	= RGBufferId;
	using RGBufferCopyDstId	= RGBufferId;
}

namespace std 
{
	template <> struct hash<adria::RGTextureId>
	{
		size_t operator()(adria::RGTextureId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGBufferId>
	{
		size_t operator()(adria::RGBufferId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureReadOnlyId>
	{
		size_t operator()(adria::RGTextureReadOnlyId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureReadWriteId>
	{
		size_t operator()(adria::RGTextureReadWriteId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGRenderTargetId>
	{
		size_t operator()(adria::RGRenderTargetId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGDepthStencilId>
	{
		size_t operator()(adria::RGDepthStencilId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
}