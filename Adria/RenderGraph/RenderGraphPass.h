#pragma once
#include <functional>
#include "RenderGraphResources.h"
#include "../Utilities/EnumUtil.h"
#include "../Utilities/HashSet.h"
#include "../Utilities/HashMap.h"


namespace adria
{
	enum class ERGPassType : uint8
	{
		Graphics,
		Compute,
		ComputeAsync,
		Copy
	};
	enum class ERGPassFlags : uint32
	{
		None = 0x00,
		ForceNoCull = 0x01,						//RGPass cannot be culled by Render Graph
		AllowUAVWrites = 0x02,					//Allow uav writes, only makes sense if LegacyRenderPassEnabled is disabled
		SkipAutoRenderPass = 0x04,				//RGPass will manage render targets by himself
		LegacyRenderPassEnabled = 0x08,			//Don't use DX12 Render Passes, use OMSetRenderTargets
		SkipDependencyWhenWriting = 0x10	    //When writing to the same resource as some previous pass, skip adding dependency between those passes
	};
	DEFINE_ENUM_BIT_OPERATORS(ERGPassFlags);

	enum ERGReadAccess : uint8
	{
		ReadAccess_PixelShader,
		ReadAccess_NonPixelShader,
		ReadAccess_AllShader
	};

	enum class ERGLoadAccessOp : uint8
	{
		Discard,
		Preserve,
		Clear,
		NoAccess
	};

	enum class ERGStoreAccessOp : uint8
	{
		Discard,
		Preserve,
		Resolve,
		NoAccess
	};

	inline constexpr uint8 CombineAccessOps(ERGLoadAccessOp load_op, ERGStoreAccessOp store_op)
	{
		return (uint8)load_op << 2 | (uint8)store_op;
	}
	enum class ERGLoadStoreAccessOp : uint8
	{
		Discard_Discard = CombineAccessOps(ERGLoadAccessOp::Discard, ERGStoreAccessOp::Discard),
		Discard_Preserve = CombineAccessOps(ERGLoadAccessOp::Discard, ERGStoreAccessOp::Preserve),
		Clear_Preserve = CombineAccessOps(ERGLoadAccessOp::Clear, ERGStoreAccessOp::Preserve),
		Preserve_Preserve = CombineAccessOps(ERGLoadAccessOp::Preserve, ERGStoreAccessOp::Preserve),
		Clear_Discard = CombineAccessOps(ERGLoadAccessOp::Clear, ERGStoreAccessOp::Discard),
		Preserve_Discard = CombineAccessOps(ERGLoadAccessOp::Preserve, ERGStoreAccessOp::Discard),
		Clear_Resolve = CombineAccessOps(ERGLoadAccessOp::Clear, ERGStoreAccessOp::Resolve),
		Preserve_Resolve = CombineAccessOps(ERGLoadAccessOp::Preserve, ERGStoreAccessOp::Resolve),
		Discard_Resolve = CombineAccessOps(ERGLoadAccessOp::Discard, ERGStoreAccessOp::Resolve),
		NoAccess_NoAccess = CombineAccessOps(ERGLoadAccessOp::NoAccess, ERGStoreAccessOp::NoAccess),
	};

	inline constexpr void SplitAccessOp(ERGLoadStoreAccessOp load_store_op, ERGLoadAccessOp& load_op, ERGStoreAccessOp& store_op)
	{
		store_op = static_cast<ERGStoreAccessOp>((uint8)load_store_op & 0b11);
		load_op  = static_cast<ERGLoadAccessOp>(((uint8)load_store_op >> 2) & 0b11);
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
			ERGLoadStoreAccessOp render_target_access;
		};
		struct DepthStencilInfo
		{
			RGDepthStencilId depth_stencil_handle;
			ERGLoadStoreAccessOp depth_access;
			ERGLoadStoreAccessOp stencil_access;
			bool readonly;
		};

	public:
		explicit RenderGraphPassBase(char const* name, ERGPassType type = ERGPassType::Graphics, ERGPassFlags flags = ERGPassFlags::None)
			: name(name), type(type), flags(flags) {}
		virtual ~RenderGraphPassBase() = default;

	protected:

		virtual void Setup(RenderGraphBuilder&) = 0;
		virtual void Execute(RenderGraphResources&, GraphicsDevice*, RGCommandList*) const = 0;

		bool IsCulled() const { return CanBeCulled() && ref_count == 0; }
		bool CanBeCulled() const { return !HasAnyFlag(flags, ERGPassFlags::ForceNoCull); }
		bool SkipAutoRenderPassSetup() const { return HasAnyFlag(flags, ERGPassFlags::SkipAutoRenderPass); }
		bool UseLegacyRenderPasses() const { return HasAnyFlag(flags, ERGPassFlags::LegacyRenderPassEnabled); }
		bool AllowUAVWrites() const { return HasAnyFlag(flags, ERGPassFlags::AllowUAVWrites); }
		bool SkipDependencyWhenWriting() const { return HasAnyFlag(flags, ERGPassFlags::SkipDependencyWhenWriting); };
	private:
		std::string name;
		size_t ref_count = 0ull;
		ERGPassType type;
		ERGPassFlags flags = ERGPassFlags::None;

		HashSet<RGTextureId> texture_creates;
		HashSet<RGTextureId> texture_reads;
		HashSet<RGTextureId> texture_writes;
		HashSet<RGTextureId> texture_destroys;
		HashMap<RGTextureId, RGResourceState> texture_state_map;
		
		HashSet<RGBufferId> buffer_creates;
		HashSet<RGBufferId> buffer_reads;
		HashSet<RGBufferId> buffer_writes;
		HashSet<RGBufferId> buffer_destroys;
		HashMap<RGBufferId, RGResourceState> buffer_state_map;

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
		using ExecuteFunc = std::function<void(PassData const&, RenderGraphResources&, GraphicsDevice*, RGCommandList*)>;

	public:
		RenderGraphPass(char const* name, SetupFunc&& setup, ExecuteFunc&& execute, ERGPassType type = ERGPassType::Graphics, ERGPassFlags flags = ERGPassFlags::None)
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

		void Execute(RenderGraphResources& resources, GraphicsDevice* dev, RGCommandList* ctx) const override
		{
			ADRIA_ASSERT(setup != nullptr && "execute function is null!");
			execute(data, resources, dev, ctx);
		}
	};

	template<>
	class RenderGraphPass<void> final : public RenderGraphPassBase
	{
	public:
		using SetupFunc = std::function<void(RenderGraphBuilder&)>;
		using ExecuteFunc = std::function<void(RenderGraphResources&, GraphicsDevice*, RGCommandList*)>;

	public:
		RenderGraphPass(char const* name, SetupFunc&& setup, ExecuteFunc&& execute, ERGPassType type = ERGPassType::Graphics, ERGPassFlags flags = ERGPassFlags::None)
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

		void Execute(RenderGraphResources& resources, GraphicsDevice* dev, RGCommandList* ctx) const override
		{
			ADRIA_ASSERT(setup != nullptr && "execute function is null!");
			execute(resources, dev, ctx);
		}
	};

	template<typename PassData>
	using RGPass = RenderGraphPass<PassData>;
};