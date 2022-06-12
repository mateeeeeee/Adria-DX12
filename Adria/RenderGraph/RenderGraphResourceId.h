#pragma once
#include <compare>
#include "../Core/Definitions.h"

namespace adria
{
	enum class ERGResourceType : uint8
	{
		Buffer,
		Texture
	};

	enum class ERGResourceMode : uint8
	{
		CopySrc, 
		CopyDst,
		IndirectArgs,
		Vertex,
		Index,
		Constant
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
	struct TypedRenderGraphResourceId : RGResourceId
	{
		using RGResourceId::RGResourceId;
	};

	using RGBufferId = TypedRenderGraphResourceId<ERGResourceType::Buffer>;
	using RGTextureId = TypedRenderGraphResourceId<ERGResourceType::Texture>;

	template<ERGResourceMode Mode>
	struct RGTextureModeId : RGTextureId
	{
		using RGTextureId::RGTextureId;
	private:
		friend class RenderGraphBuilder;
		friend class RenderGraph;

		RGTextureModeId(RGTextureId const& id) : RGTextureId(id) {}
	};

	template<ERGResourceMode Mode>
	struct RGBufferModeId : RGBufferId
	{
		using RGBufferId::RGBufferId;
	private:
		friend class RenderGraphBuilder;
		friend class RenderGraph;

		RGBufferModeId(RGBufferId const& id) : RGBufferId(id) {}
	};

	using RGTextureCopySrcId = RGTextureModeId<ERGResourceMode::CopySrc>;
	using RGTextureCopyDstId = RGTextureModeId<ERGResourceMode::CopyDst>;

	using RGBufferCopySrcId = RGBufferModeId<ERGResourceMode::CopySrc>;
	using RGBufferCopyDstId = RGBufferModeId<ERGResourceMode::CopyDst>;
	using RGBufferIndirectArgsId = RGBufferModeId<ERGResourceMode::IndirectArgs>;
	using RGBufferVertexId = RGBufferModeId<ERGResourceMode::Vertex>;
	using RGBufferIndexId = RGBufferModeId<ERGResourceMode::Index>;
	using RGBufferConstantId = RGBufferModeId<ERGResourceMode::Constant>;

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	struct RenderGraphResourceDescriptorId
	{
		inline constexpr static uint64 invalid_id = uint64(-1);

		RenderGraphResourceDescriptorId() : id(invalid_id) {}
		RenderGraphResourceDescriptorId(size_t view_id, RenderGraphResourceId resource_handle)
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
		auto operator<=>(RenderGraphResourceDescriptorId const&) const = default;

		uint64 id;
	};

	enum class ERGDescriptorType : uint8
	{
		ReadOnly,
		ReadWrite,
		RenderTarget,
		DepthStencil
	};

	template<ERGResourceType ResourceType, ERGDescriptorType ResourceViewType>
	struct TypedRenderGraphResourceDescriptorId : RenderGraphResourceDescriptorId 
	{
		using RenderGraphResourceDescriptorId::RenderGraphResourceDescriptorId;

		auto GetResourceId() const
		{
			if constexpr (ResourceType == ERGResourceType::Buffer) return RGBufferId(RenderGraphResourceDescriptorId::GetResourceId());
			else if constexpr (ResourceType == ERGResourceType::Texture) return RGTextureId(RenderGraphResourceDescriptorId::GetResourceId());
		}
	};

	using RGRenderTargetId		 = TypedRenderGraphResourceDescriptorId<ERGResourceType::Texture, ERGDescriptorType::RenderTarget>;
	using RGDepthStencilId		 = TypedRenderGraphResourceDescriptorId<ERGResourceType::Texture, ERGDescriptorType::DepthStencil>;
	using RGTextureReadOnlyId	 = TypedRenderGraphResourceDescriptorId<ERGResourceType::Texture,  ERGDescriptorType::ReadOnly>;
	using RGTextureReadWriteId	 = TypedRenderGraphResourceDescriptorId<ERGResourceType::Texture,  ERGDescriptorType::ReadWrite>;

	using RGBufferReadOnlyId	 = TypedRenderGraphResourceDescriptorId<ERGResourceType::Buffer,   ERGDescriptorType::ReadOnly>;
	using RGBufferReadWriteId	 = TypedRenderGraphResourceDescriptorId<ERGResourceType::Buffer,   ERGDescriptorType::ReadWrite>;

	///////////////////////////////////////////////////////////////////////////////////////////////////////

	struct RGAllocationId : RGResourceId
	{
		using RGResourceId::RGResourceId;
	};
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

	template <> struct hash<adria::RGBufferReadOnlyId>
	{
		size_t operator()(adria::RGBufferReadOnlyId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};

	template <> struct hash<adria::RGBufferReadWriteId>
	{
		size_t operator()(adria::RGBufferReadWriteId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
}