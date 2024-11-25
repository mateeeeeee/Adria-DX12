#pragma once
#include <compare>

namespace adria
{
	enum class RenderGraphResourceType : Uint8
	{
		Buffer,
		Texture
	};
	using RGResourceType = RenderGraphResourceType;

	enum class RenderGraphResourceMode : Uint8
	{
		CopySrc,
		CopyDst,
		IndirectArgs,
		Vertex,
		Index,
		Constant
	};
	using RGResourceMode = RenderGraphResourceMode;

	struct RenderGraphResourceId
	{
		inline constexpr static Uint32 invalid_id = Uint32(-1);

		RenderGraphResourceId() : id(invalid_id) {}
		RenderGraphResourceId(RenderGraphResourceId const&) = default;
		explicit RenderGraphResourceId(Uint64 _id) : id(static_cast<Uint32>(_id)) {}

		void Invalidate() { id = invalid_id; }
		Bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceId const&) const = default;

		Uint32 id;
	};
	using RGResourceId = RenderGraphResourceId;

	template<RGResourceType ResourceType>
	struct TypedRenderGraphResourceId : RGResourceId
	{
		using RGResourceId::RGResourceId;
	};

	using RGBufferId = TypedRenderGraphResourceId<RGResourceType::Buffer>;
	using RGTextureId = TypedRenderGraphResourceId<RGResourceType::Texture>;

	template<RGResourceMode Mode>
	struct RenderGraphTextureModeId : RGTextureId
	{
		using RGTextureId::RGTextureId;
	private:
		friend class RenderGraphBuilder;
		friend class RenderGraph;

		RenderGraphTextureModeId(RGTextureId const& id) : RGTextureId(id) {}
	};
	template<RGResourceMode Mode>
	using RGTextureModeId = RenderGraphTextureModeId<Mode>;

	template<RGResourceMode Mode>
	struct RenderGraphBufferModeId : RGBufferId
	{
		using RGBufferId::RGBufferId;
	private:
		friend class RenderGraphBuilder;
		friend class RenderGraph;

		RenderGraphBufferModeId(RGBufferId const& id) : RGBufferId(id) {}
	};
	template<RGResourceMode Mode>
	using RGBufferModeId = RenderGraphBufferModeId<Mode>;


	using RGTextureCopySrcId = RGTextureModeId<RGResourceMode::CopySrc>;
	using RGTextureCopyDstId = RGTextureModeId<RGResourceMode::CopyDst>;

	using RGBufferCopySrcId = RGBufferModeId<RGResourceMode::CopySrc>;
	using RGBufferCopyDstId = RGBufferModeId<RGResourceMode::CopyDst>;
	using RGBufferIndirectArgsId = RGBufferModeId<RGResourceMode::IndirectArgs>;
	using RGBufferVertexId = RGBufferModeId<RGResourceMode::Vertex>;
	using RGBufferIndexId = RGBufferModeId<RGResourceMode::Index>;
	using RGBufferConstantId = RGBufferModeId<RGResourceMode::Constant>;


	struct RenderGraphResourceDescriptorId
	{
		inline constexpr static Uint64 invalid_id = Uint64(-1);

		RenderGraphResourceDescriptorId() : id(invalid_id) {}
		RenderGraphResourceDescriptorId(Uint64 view_id, RenderGraphResourceId resource_handle)
			: id(invalid_id)
		{
			Uint32 _resource_id = resource_handle.id;
			id = (view_id << 32) | _resource_id;
		}

		Uint64 GetViewId() const { return (id >> 32); };
		Uint64 GetResourceId() const
		{
			return (Uint64)static_cast<Uint32>(id);
		};

		RenderGraphResourceId operator*() const
		{
			return RenderGraphResourceId(GetResourceId());
		}

		void Invalidate() { id = invalid_id; }
		Bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceDescriptorId const&) const = default;

		Uint64 id;
	};

	enum class RenderGraphDescriptorType : Uint8
	{
		ReadOnly,
		ReadWrite,
		RenderTarget,
		DepthStencil
	};
	using RGDescriptorType = RenderGraphDescriptorType;

	template<RGResourceType ResourceType, RGDescriptorType DescriptorType>
	struct TypedRenderGraphResourceDescriptorId : RenderGraphResourceDescriptorId
	{
		using RenderGraphResourceDescriptorId::RenderGraphResourceDescriptorId;
		using RenderGraphResourceDescriptorId::operator*;

		auto GetResourceId() const
		{
			if constexpr (ResourceType == RGResourceType::Buffer) return RGBufferId(RenderGraphResourceDescriptorId::GetResourceId());
			else if constexpr (ResourceType == RGResourceType::Texture) return RGTextureId(RenderGraphResourceDescriptorId::GetResourceId());
		}

		auto operator*() const
		{
			if constexpr (ResourceType == RGResourceType::Buffer) return RGBufferId(GetResourceId());
			else if constexpr (ResourceType == RGResourceType::Texture) return	RGTextureId(GetResourceId());
		}
	};

	using RGRenderTargetId		 = TypedRenderGraphResourceDescriptorId<RGResourceType::Texture, RGDescriptorType::RenderTarget>;
	using RGDepthStencilId		 = TypedRenderGraphResourceDescriptorId<RGResourceType::Texture, RGDescriptorType::DepthStencil>;
	using RGTextureReadOnlyId	 = TypedRenderGraphResourceDescriptorId<RGResourceType::Texture, RGDescriptorType::ReadOnly>;
	using RGTextureReadWriteId	 = TypedRenderGraphResourceDescriptorId<RGResourceType::Texture, RGDescriptorType::ReadWrite>;

	using RGBufferReadOnlyId	 = TypedRenderGraphResourceDescriptorId<RGResourceType::Buffer,  RGDescriptorType::ReadOnly>;
	using RGBufferReadWriteId	 = TypedRenderGraphResourceDescriptorId<RGResourceType::Buffer,  RGDescriptorType::ReadWrite>;
}

namespace std
{
	template <> struct hash<adria::RGTextureId>
	{
		adria::Uint64 operator()(adria::RGTextureId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGBufferId>
	{
		adria::Uint64 operator()(adria::RGBufferId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureReadOnlyId>
	{
		adria::Uint64 operator()(adria::RGTextureReadOnlyId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGTextureReadWriteId>
	{
		adria::Uint64 operator()(adria::RGTextureReadWriteId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGRenderTargetId>
	{
		adria::Uint64 operator()(adria::RGRenderTargetId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
	template <> struct hash<adria::RGDepthStencilId>
	{
		adria::Uint64 operator()(adria::RGDepthStencilId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};

	template <> struct hash<adria::RGBufferReadOnlyId>
	{
		adria::Uint64 operator()(adria::RGBufferReadOnlyId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};

	template <> struct hash<adria::RGBufferReadWriteId>
	{
		adria::Uint64 operator()(adria::RGBufferReadWriteId const& h) const
		{
			return hash<decltype(h.id)>()(h.id);
		}
	};
}