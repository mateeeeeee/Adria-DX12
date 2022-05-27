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

	struct RenderGraphResourceHandle
	{
		inline constexpr static uint32 invalid_id = uint32(-1);

		RenderGraphResourceHandle() : id(invalid_id) {}
		RenderGraphResourceHandle(size_t _id) : id(static_cast<uint32>(_id)) {}

		void Invalidate() { id = invalid_id; }
		bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceHandle const&) const = default;

		uint32 id;
	};

	struct RenderGraphResourceViewHandle
	{
		inline constexpr static uint64 invalid_id = uint64(-1);

		RenderGraphResourceViewHandle() : id(invalid_id) {}
		RenderGraphResourceViewHandle(size_t view_id, RenderGraphResourceHandle resource_handle)
			: id(invalid_id)
		{
			uint32 _resource_id = resource_handle.id;
			id = (view_id << 32) | _resource_id;
		}

		size_t GetViewId() const { return (id >> 32); };
		RenderGraphResourceHandle GetResourceHandle() const 
		{
			return RenderGraphResourceHandle((size_t)static_cast<uint32>(id));
		};

		void Invalidate() { id = invalid_id; }
		bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceViewHandle const&) const = default;

		uint64 id;
	};

	template<ERGResourceType ResourceType>
	struct TypedRenderGraphResourceHandle : RenderGraphResourceHandle {};

	using RGBufferHandle = TypedRenderGraphResourceHandle<ERGResourceType::Buffer>;
	using RGTextureHandle = TypedRenderGraphResourceHandle<ERGResourceType::Texture>;

	template<ERGResourceType ResourceType, ERGResourceViewType ResourceViewType>
	struct TypedRenderGraphResourceViewHandle : RenderGraphResourceViewHandle {};

	using RGTextureHandleSRV = TypedRenderGraphResourceViewHandle<ERGResourceType::Texture, ERGResourceViewType::SRV>;
	using RGTextureHandleUAV = TypedRenderGraphResourceViewHandle<ERGResourceType::Texture, ERGResourceViewType::UAV>;
	using RGTextureHandleRTV = TypedRenderGraphResourceViewHandle<ERGResourceType::Texture, ERGResourceViewType::RTV>;
	using RGTextureHandleDSV = TypedRenderGraphResourceViewHandle<ERGResourceType::Texture, ERGResourceViewType::DSV>;
	using RGBufferHandleDSV = TypedRenderGraphResourceViewHandle<ERGResourceType::Buffer, ERGResourceViewType::SRV>;
	using RGBufferHandleDSV = TypedRenderGraphResourceViewHandle<ERGResourceType::Buffer, ERGResourceViewType::UAV>;
}

namespace std 
{
	template <> struct hash<adria::RGTextureHandle>
	{
		size_t operator()(adria::RGTextureHandle const& h) const
		{
			return hash<adria::uint32>()(h.id);
		}
	};
	template <> struct hash<adria::RGBufferHandle>
	{
		size_t operator()(adria::RGBufferHandle const& h) const
		{
			return hash<adria::uint32>()(h.id);
		}
	};
}