#pragma once


namespace adria
{
	struct RenderGraphResourceHandle
	{
		inline constexpr static size_t invalid_id = size_t(-1);

		RenderGraphResourceHandle() : id(invalid_id) {}
		RenderGraphResourceHandle(size_t _id) : id(_id) {}

		bool IsValid() const { return id != invalid_id; }
		auto operator<=>(RenderGraphResourceHandle const&) const = default;

		size_t id;
	};

	using RGResourceHandle = RenderGraphResourceHandle;
}

namespace std 
{
	template <> struct hash<adria::RenderGraphResourceHandle>
	{
		size_t operator()(adria::RenderGraphResourceHandle const& h) const
		{
			return hash<size_t>()(h.id);
		}
	};
}