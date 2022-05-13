#pragma once

#include <functional>
#include <unordered_set>
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
		ComputeAsync
	};
	enum class ERGPassFlags : uint32
	{
		None = 0x00,
		ForceNoCull = 0x01
	};
	DEFINE_ENUM_BIT_OPERATORS(ERGPassFlags);

	class RenderGraph;
	class RenderGraphBuilder;


	class RenderGraphPassBase
	{
		friend RenderGraph;
		friend RenderGraphBuilder;

	public:
		explicit RenderGraphPassBase(char const* name, ERGPassType type = ERGPassType::Graphics, ERGPassFlags flags = ERGPassFlags::None)
			: name(name), type(type), flags(flags) {}
		virtual ~RenderGraphPassBase() = default;

	protected:

		virtual void Setup(RenderGraphBuilder&) = 0;
		virtual void Execute(RenderGraphResources&, void*, void*) const = 0;

		bool CanBeCulled() const { return (flags & ERGPassFlags::ForceNoCull) == ERGPassFlags::None; }
		bool IsCulled() const { return ref_count == 0; }

	protected:
		std::string name;
		size_t ref_count = 0ull;
		ERGPassType type;
		ERGPassFlags flags;
		std::unordered_set<RGResourceHandle> reads;
		std::unordered_set<RGResourceHandle> writes;
		std::unordered_set<RGResourceHandle> creates;
		std::unordered_set<RGResourceHandle> destroy;
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

		void Execute(RenderGraphResources& rsrcs, void* ctx, void* dev) const override
		{
			ADRIA_ASSERT(setup != nullptr && "execute function is null!");
			execute(data, rsrcs, ctx, dev);
		}
	};

	template<typename PassData>
	using RGPass = RenderGraphPass<PassData>;
};