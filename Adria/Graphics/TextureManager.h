#pragma once
#include <string>
#include <array>
#include <d3d12.h>
#include <wrl.h>
#include <variant>
#include <unordered_map>
#include <memory>
#include "MipsGenerator.h"
#include "ResourceBarriers.h"

namespace adria
{
	class GraphicsCoreDX12;
	class DescriptorHeap;
	
	using TEXTURE_HANDLE = size_t;
	inline constexpr TEXTURE_HANDLE const INVALID_TEXTURE_HANDLE = TEXTURE_HANDLE(-1);

	class TextureManager
	{
	public:
		TextureManager(GraphicsCoreDX12* gfx, UINT max_textures);

		[[nodiscard]] TEXTURE_HANDLE LoadTexture(std::wstring const& name);

		[[nodiscard]] TEXTURE_HANDLE LoadCubemap(std::wstring const& name);

		[[nodiscard]] TEXTURE_HANDLE LoadCubemap(std::array<std::wstring, 6> const& cubemap_textures);

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle(TEXTURE_HANDLE tex_handle);

		void TransitionTexture(TEXTURE_HANDLE handle, ResourceBarriers& barrier,
			D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

		void SetMipMaps(bool);

		void GenerateAllMips()
		{
			mips_generator->Generate(gfx->DefaultCommandList());
		}

	private:
		GraphicsCoreDX12* gfx;
		std::unique_ptr<DescriptorHeap> texture_srv_heap = nullptr;
		std::unique_ptr<MipsGenerator> mips_generator = nullptr;

		TEXTURE_HANDLE handle = 0;
		std::unordered_map<TEXTURE_HANDLE, Microsoft::WRL::ComPtr<ID3D12Resource>> texture_map{};
		std::unordered_map<std::variant<std::wstring, std::string>, TEXTURE_HANDLE> loaded_textures{};
		bool mipmaps = true;

	private:

		TEXTURE_HANDLE LoadDDSTexture(std::wstring const& texture_path);

		TEXTURE_HANDLE LoadWICTexture(std::wstring const& texture_path);

		TEXTURE_HANDLE LoadTexture_HDR_TGA_PIC(std::string const& texture_path);
	};
}