#pragma once
#include <memory>
#include <d3d12.h>
#include "GfxMacros.h"
#include "Utilities/Ref.h"

namespace adria
{
	enum class IndirectCommandType : Uint8
	{
		Draw,
		DrawIndexed,
		Dispatch,
		DispatchMesh
	};
	template<IndirectCommandType>
	struct IndirectCommandTraits;

	template<>
	struct IndirectCommandTraits<IndirectCommandType::Draw>
	{
		static constexpr D3D12_INDIRECT_ARGUMENT_TYPE ArgumentType = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
		static constexpr Uint32 Stride = sizeof(D3D12_DRAW_ARGUMENTS);
	};
	template<>
	struct IndirectCommandTraits<IndirectCommandType::DrawIndexed>
	{
		static constexpr D3D12_INDIRECT_ARGUMENT_TYPE ArgumentType = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		static constexpr Uint32 Stride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	};
	template<>
	struct IndirectCommandTraits<IndirectCommandType::Dispatch>
	{
		static constexpr D3D12_INDIRECT_ARGUMENT_TYPE ArgumentType = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
		static constexpr Uint32 Stride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	};

	template<>
	struct IndirectCommandTraits<IndirectCommandType::DispatchMesh>
	{
		static constexpr D3D12_INDIRECT_ARGUMENT_TYPE ArgumentType = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
		static constexpr Uint32 Stride = sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
	};

	template<IndirectCommandType Type>
	class IndirectCommandSignature
	{
	public:
		explicit IndirectCommandSignature(ID3D12Device* device)
		{
			D3D12_COMMAND_SIGNATURE_DESC desc{};
			D3D12_INDIRECT_ARGUMENT_DESC argument_desc{};
			desc.NumArgumentDescs = 1;
			desc.pArgumentDescs = &argument_desc;
			desc.ByteStride = IndirectCommandTraits<Type>::Stride;
			argument_desc.Type = IndirectCommandTraits<Type>::ArgumentType;
			GFX_CHECK_HR(device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(cmd_signature.GetAddressOf())));
		}
		operator ID3D12CommandSignature* () const
		{
			return cmd_signature.Get();
		}
	private:
		Ref<ID3D12CommandSignature> cmd_signature;
	};

	using DrawIndirectSignature			= IndirectCommandSignature<IndirectCommandType::Draw>;
	using DrawIndexedIndirectSignature	= IndirectCommandSignature<IndirectCommandType::DrawIndexed>;
	using DispatchIndirectSignature		= IndirectCommandSignature<IndirectCommandType::Dispatch>;
	using DispatchMeshIndirectSignature = IndirectCommandSignature<IndirectCommandType::DispatchMesh>;
}