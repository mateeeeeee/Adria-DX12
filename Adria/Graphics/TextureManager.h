#pragma once
#include <string>
#include <array>
#include <d3d12.h>
#include <wrl.h>
#include <variant>
#include <memory>
#include "MipsGenerator.h"
#include "ResourceBarrierBatch.h"
#include "../Utilities/HashMap.h"

namespace adria
{
	class GraphicsDevice;
	class DescriptorHeap;
	
	using TextureHandle = size_t;
	inline constexpr TextureHandle INVALID_TEXTURE_HANDLE = TextureHandle(-1);

	class TextureManager
	{
		friend class Renderer;
		friend class Renderer;

	public:
		TextureManager(GraphicsDevice* gfx, UINT max_textures);

		[[nodiscard]] TextureHandle LoadTexture(std::wstring const& name);

		[[nodiscard]] TextureHandle LoadCubemap(std::wstring const& name);

		[[nodiscard]] TextureHandle LoadCubemap(std::array<std::wstring, 6> const& cubemap_textures);

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle(TextureHandle tex_handle);

		ID3D12Resource* Resource(TextureHandle handle) const
		{
			if (handle == INVALID_TEXTURE_HANDLE) return nullptr;
			else if (auto it = texture_map.find(handle); it != texture_map.end()) return it->second.Get();
			else return nullptr;
		}

		void SetMipMaps(bool);

		void GenerateAllMips()
		{
			mips_generator->Generate(gfx->GetDefaultCommandList());
		}

	private:
		GraphicsDevice* gfx;
		std::unique_ptr<DescriptorHeap> texture_srv_heap = nullptr;
		std::unique_ptr<MipsGenerator> mips_generator = nullptr;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> equirect_root_signature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> equirect_pso;
		TextureHandle handle = 0;
		HashMap<TextureHandle, Microsoft::WRL::ComPtr<ID3D12Resource>> texture_map{};
		HashMap<std::variant<std::wstring, std::string>, TextureHandle> loaded_textures{};
		bool mipmaps = true;

	private:

		TextureHandle LoadDDSTexture(std::wstring const& texture_path);

		TextureHandle LoadWICTexture(std::wstring const& texture_path);

		TextureHandle LoadTexture_HDR_TGA_PIC(std::string const& texture_path);
	};
}