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
	class Texture;
	class DescriptorHeap;
	
	using TextureHandle = size_t;
	inline constexpr TextureHandle INVALID_TEXTURE_HANDLE = TextureHandle(0);

	class TextureManager
	{
		using TextureName = std::variant<std::wstring, std::string>;
	public:
		TextureManager(GraphicsDevice* gfx, UINT max_textures);

		[[nodiscard]] TextureHandle LoadTexture(std::wstring const& name);
		[[nodiscard]] TextureHandle LoadCubemap(std::wstring const& name);
		[[nodiscard]] TextureHandle LoadCubemap(std::array<std::wstring, 6> const& cubemap_textures);
		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSRV(TextureHandle tex_handle);
		Texture* GetTexture(TextureHandle handle) const;
		void EnableMipMaps(bool);
		void OnSceneInitialized();

	private:
		GraphicsDevice* gfx;
		std::unique_ptr<MipsGenerator> mips_generator = nullptr;

		HashMap<TextureName, TextureHandle> loaded_textures{};
		HashMap<TextureHandle, std::unique_ptr<Texture>> texture_map{};
		TextureHandle handle = 0;
		bool mipmaps = true;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> equirect_root_signature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> equirect_pso;
	private:

		TextureHandle LoadDDSTexture(std::wstring const& texture_path);
		TextureHandle LoadWICTexture(std::wstring const& texture_path);
		TextureHandle LoadTexture_HDR_TGA_PIC(std::string const& texture_path);
	};
}