#pragma once

#include <functional>
#include <unordered_set>
#include <unordered_map>
#include "RenderGraphResourceHandle.h"
#include "RenderGraphResources.h"
#include "../Core/Definitions.h"
#include "../Utilities/EnumUtil.h"

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
		ForceNoCull = 0x01,
		AllowUAVWrites = 0x02,
		DisableRenderPass = 0x04
	};
	DEFINE_ENUM_BIT_OPERATORS(ERGPassFlags);

	enum ERGReadFlag : uint32
	{
		ReadFlag_PixelShaderAccess,
		ReadFlag_NonPixelShaderAccess,
		ReadFlag_AllPixelShaderAccess,
		ReadFlag_IndirectArgument,
		ReadFlag_CopySrc
	};

	enum ERGWriteFlag : uint32
	{

		WriteFlag_UnorderedAccess,
		WriteFlag_CopyDst
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
		store_op = static_cast<ERGStoreAccessOp>((uint8)load_store_op & 0x11);
		load_op  = static_cast<ERGLoadAccessOp>(((uint8)load_store_op >> 2) & 0x11);
	}


	class RenderGraph;
	class RenderGraphBuilder;

	class RenderGraphPassBase
	{
		friend RenderGraph;
		friend RenderGraphBuilder;

		struct RenderTargetInfo
		{
			RGResourceHandle render_target_handle;
			ERGLoadStoreAccessOp render_target_access;
		};

		struct DepthStencilInfo
		{
			RGResourceHandle depth_stencil_handle;
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
		virtual void Execute(RenderGraphResources&, void*, void*) const = 0;

		bool CanBeCulled() const { return HasAnyFlag(flags, ERGPassFlags::ForceNoCull); }
		bool IsCulled() const { return ref_count == 0; }

	private:
		std::string name;
		size_t ref_count = 0ull;
		ERGPassType type;
		ERGPassFlags flags;
		std::unordered_set<RGResourceHandle> creates;
		std::unordered_set<RGResourceHandle> reads;
		std::unordered_set<RGResourceHandle> writes;
		std::unordered_set<RGResourceHandle> destroy;

		std::unordered_map<RGResourceHandle, D3D12_RESOURCE_STATES> resource_state_map;
		std::vector<RenderTargetInfo> render_targets;
		std::optional<DepthStencilInfo> depth_stencil = std::nullopt;
	};
	using RGPassBase = RenderGraphPassBase;

	template<typename PassData>
	class RenderGraphPass final : public RenderGraphPassBase
	{
	public:
		using SetupFunc = std::function<void(PassData&, RenderGraphBuilder&)>;
		using ExecuteFunc = std::function<void(PassData const&, RenderGraphResources&, void*, void*)>;

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

		void Execute(RenderGraphResources& resources, void* ctx, void* dev) const override
		{
			ADRIA_ASSERT(setup != nullptr && "execute function is null!");
			execute(data, resources, ctx, dev);
		}
	};

	template<typename PassData>
	using RGPass = RenderGraphPass<PassData>;
};