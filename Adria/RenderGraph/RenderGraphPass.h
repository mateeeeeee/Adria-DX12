#pragma once
#include <functional>
#include "RenderGraphContext.h"
#include "../Utilities/EnumUtil.h"
#include "../Utilities/HashSet.h"
#include "../Utilities/HashMap.h"


namespace adria
{
	enum class RGPassType : uint8
	{
		Graphics,
		Compute,
		ComputeAsync,
		Copy
	};
	enum class RGPassFlags : uint32
	{
		None = 0x00,
		ForceNoCull = 0x01,						//RGPass cannot be culled by Render Graph
		AllowUAVWrites = 0x02,					//Allow uav writes, only makes sense if LegacyRenderPassEnabled is disabled
		SkipAutoRenderPass = 0x04,				//RGPass will manage render targets by himself
		LegacyRenderPassEnabled = 0x08,			//Don't use DX12 Render Passes, use OMSetRenderTargets
		ActAsCreatorWhenWriting = 0x10			//When writing to a resource, avoid forcing dependency by acting as a creator
	};
	DEFINE_ENUM_BIT_OPERATORS(RGPassFlags);

	enum RGReadAccess : uint8
	{
		ReadAccess_PixelShader,
		ReadAccess_NonPixelShader,
		ReadAccess_AllShader
	};

	enum class RGLoadAccessOp : uint8
	{
		Discard,
		Preserve,
		Clear,
		NoAccess
	};

	enum class RGStoreAccessOp : uint8
	{
		Discard,
		Preserve,
		Resolve,
		NoAccess
	};

	inline constexpr uint8 CombineAccessOps(RGLoadAccessOp load_op, RGStoreAccessOp store_op)
	{
		return (uint8)load_op << 2 | (uint8)store_op;
	}
	enum class RGLoadStoreAccessOp : uint8
	{
		Discard_Discard = CombineAccessOps(RGLoadAccessOp::Discard, RGStoreAccessOp::Discard),
		Discard_Preserve = CombineAccessOps(RGLoadAccessOp::Discard, RGStoreAccessOp::Preserve),
		Clear_Preserve = CombineAccessOps(RGLoadAccessOp::Clear, RGStoreAccessOp::Preserve),
		Preserve_Preserve = CombineAccessOps(RGLoadAccessOp::Preserve, RGStoreAccessOp::Preserve),
		Clear_Discard = CombineAccessOps(RGLoadAccessOp::Clear, RGStoreAccessOp::Discard),
		Preserve_Discard = CombineAccessOps(RGLoadAccessOp::Preserve, RGStoreAccessOp::Discard),
		Clear_Resolve = CombineAccessOps(RGLoadAccessOp::Clear, RGStoreAccessOp::Resolve),
		Preserve_Resolve = CombineAccessOps(RGLoadAccessOp::Preserve, RGStoreAccessOp::Resolve),
		Discard_Resolve = CombineAccessOps(RGLoadAccessOp::Discard, RGStoreAccessOp::Resolve),
		NoAccess_NoAccess = CombineAccessOps(RGLoadAccessOp::NoAccess, RGStoreAccessOp::NoAccess),
	};

	inline constexpr void SplitAccessOp(RGLoadStoreAccessOp load_store_op, RGLoadAccessOp& load_op, RGStoreAccessOp& store_op)
	{
		store_op = static_cast<RGStoreAccessOp>((uint8)load_store_op & 0b11);
		load_op = static_cast<RGLoadAccessOp>(((uint8)load_store_op >> 2) & 0b11);
	}

	class RenderGraph;
	class RenderGraphBuilder;

	class RenderGraphPassBase
	{
		friend RenderGraph;
		friend RenderGraphBuilder;

		struct RenderTargetInfo
		{
			RGRenderTargetId render_target_handle;
			RGLoadStoreAccessOp render_target_access;
		};
		struct DepthStencilInfo
		{
			RGDepthStencilId depth_stencil_handle;
			RGLoadStoreAccessOp depth_access;
			RGLoadStoreAccessOp stencil_access;
			bool readonly;
		};

	public:
		explicit RenderGraphPassBase(char const* name, RGPassType type = RGPassType::Graphics, RGPassFlags flags = RGPassFlags::None)
			: name(name), type(type), flags(flags) {}
		virtual ~RenderGraphPassBase() = default;

	protected:

		virtual void Setup(RenderGraphBuilder&) = 0;
		virtual void Execute(RenderGraphContext&, GfxDevice*, CommandList*) const = 0;

		bool IsCulled() const { return CanBeCulled() && ref_count == 0; }
		bool CanBeCulled() const { return !HasAnyFlag(flags, RGPassFlags::ForceNoCull); }
		bool SkipAutoRenderPassSetup() const { return HasAnyFlag(flags, RGPassFlags::SkipAutoRenderPass); }
		bool UseLegacyRenderPasses() const { return HasAnyFlag(flags, RGPassFlags::LegacyRenderPassEnabled); }
		bool AllowUAVWrites() const { return HasAnyFlag(flags, RGPassFlags::AllowUAVWrites); }
		bool ActAsCreatorWhenWriting() const { return HasAnyFlag(flags, RGPassFlags::ActAsCreatorWhenWriting); };
	private:
		std::string name;
		size_t ref_count = 0ull;
		RGPassType type;
		RGPassFlags flags = RGPassFlags::None;

		HashSet<RGTextureId> texture_creates;
		HashSet<RGTextureId> texture_reads;
		HashSet<RGTextureId> texture_writes;
		HashSet<RGTextureId> texture_destroys;
		HashMap<RGTextureId, GfxResourceState> texture_state_map;
		
		HashSet<RGBufferId> buffer_creates;
		HashSet<RGBufferId> buffer_reads;
		HashSet<RGBufferId> buffer_writes;
		HashSet<RGBufferId> buffer_destroys;
		HashMap<RGBufferId, GfxResourceState> buffer_state_map;

		std::vector<RenderTargetInfo> render_targets_info;
		std::optional<DepthStencilInfo> depth_stencil = std::nullopt;
		uint32 viewport_width = 0, viewport_height = 0;
	};
	using RGPassBase = RenderGraphPassBase;

	template<typename PassData>
	class RenderGraphPass final : public RenderGraphPassBase
	{
	public:
		using SetupFunc = std::function<void(PassData&, RenderGraphBuilder&)>;
		using ExecuteFunc = std::function<void(PassData const&, RenderGraphContext&, GfxDevice*, CommandList*)>;

	public:
		RenderGraphPass(char const* name, SetupFunc&& setup, ExecuteFunc&& execute, RGPassType type = RGPassType::Graphics, RGPassFlags flags = RGPassFlags::None)
			: RenderGraphPassBase(name, type, flags), setup(std::move(setup)), execute(std::move(execute))
		{}

		PassData const& GetPassData() const
		{
			return data;
		}

	private:
		PassData data;
		SetupFunc setup;
		ExecuteFunc execute;

	private:

		void Setup(RenderGraphBuilder& builder) override
		{
			ADRIA_ASSERT(setup != nullptr && "setup function is null!");
			setup(data, builder);
		}

		void Execute(RenderGraphContext& context, GfxDevice* dev, CommandList* ctx) const override
		{
			ADRIA_ASSERT(setup != nullptr && "execute function is null!");
			execute(data, context, dev, ctx);
		}
	};

	template<>
	class RenderGraphPass<void> final : public RenderGraphPassBase
	{
	public:
		using SetupFunc = std::function<void(RenderGraphBuilder&)>;
		using ExecuteFunc = std::function<void(RenderGraphContext&, GfxDevice*, CommandList*)>;

	public:
		RenderGraphPass(char const* name, SetupFunc&& setup, ExecuteFunc&& execute, RGPassType type = RGPassType::Graphics, RGPassFlags flags = RGPassFlags::None)
			: RenderGraphPassBase(name, type, flags), setup(std::move(setup)), execute(std::move(execute))
		{}

		void GetPassData() const
		{
			return;
		}

	private:
		SetupFunc setup;
		ExecuteFunc execute;

	private:

		void Setup(RenderGraphBuilder& builder) override
		{
			ADRIA_ASSERT(setup != nullptr && "setup function is null!");
			setup(builder);
		}

		void Execute(RenderGraphContext& context, GfxDevice* dev, CommandList* ctx) const override
		{
			ADRIA_ASSERT(setup != nullptr && "execute function is null!");
			execute(context, dev, ctx);
		}
	};

	template<typename PassData>
	using RGPass = RenderGraphPass<PassData>;
};