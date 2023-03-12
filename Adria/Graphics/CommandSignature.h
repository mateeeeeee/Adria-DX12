#pragma once
#include <memory>
#include "GfxDevice.h"

namespace adria
{
	enum class IndirectCommandType
	{
		Draw,
		DrawIndexed,
		Dispatch
	};
	template<IndirectCommandType>
	struct IndirectCommandTraits;

	template<>
	struct IndirectCommandTraits<IndirectCommandType::Draw>
	{
		static constexpr D3D12_INDIRECT_ARGUMENT_TYPE ArgumentType = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
		static constexpr UINT Stride = sizeof(D3D12_DRAW_ARGUMENTS);
	};
	template<>
	struct IndirectCommandTraits<IndirectCommandType::DrawIndexed>
	{
		static constexpr D3D12_INDIRECT_ARGUMENT_TYPE ArgumentType = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		static constexpr UINT Stride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	};
	template<>
	struct IndirectCommandTraits<IndirectCommandType::Dispatch>
	{
		static constexpr D3D12_INDIRECT_ARGUMENT_TYPE ArgumentType = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
		static constexpr UINT Stride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	};

	template<IndirectCommandType type>
	class IndirectCommandSignature
	{
	public:
		explicit IndirectCommandSignature(GfxDevice* gfx)
		{
			D3D12_COMMAND_SIGNATURE_DESC desc{};
			D3D12_INDIRECT_ARGUMENT_DESC argument_desc{};
			desc.NumArgumentDescs = 1;
			desc.pArgumentDescs = &argument_desc;
			desc.ByteStride = IndirectCommandTraits<type>::Stride;
			argument_desc.Type = IndirectCommandTraits<type>::ArgumentType;
			BREAK_IF_FAILED(gfx->GetDevice()->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(cmd_signature.GetAddressOf())));
		}
		operator ID3D12CommandSignature* () const
		{
			return cmd_signature.Get();
		}
	private:
		ArcPtr<ID3D12CommandSignature> cmd_signature;
	};

	using DrawIndirectSignature			= IndirectCommandSignature<IndirectCommandType::Draw>;
	using DrawIndexedIndirectSignature	= IndirectCommandSignature<IndirectCommandType::DrawIndexed>;
	using DispatchIndirectSignature		= IndirectCommandSignature<IndirectCommandType::Dispatch>;
}