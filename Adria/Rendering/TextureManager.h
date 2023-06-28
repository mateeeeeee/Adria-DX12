#pragma once
#include <string>
#include <array>
#include <d3d12.h>
#include <variant>
#include <memory>
#include "Graphics/GfxDescriptor.h"
#include "Utilities/Singleton.h"
#include "Utilities/AutoRefCountPtr.h"

namespace adria
{
	class GfxDevice;
	class GfxTexture;

	using TextureHandle = size_t;
	inline constexpr TextureHandle INVALID_TEXTURE_HANDLE = TextureHandle(0);

	class TextureManager : public Singleton<TextureManager>
	{
		friend class Singleton<TextureManager>;
		using TextureName = std::string;

	public:

		void Initialize(GfxDevice* gfx, uint32 max_textures);
		void Destroy();

		[[nodiscard]] TextureHandle LoadTexture(std::string_view path);
		[[nodiscard]] TextureHandle LoadCubemap(std::array<std::string, 6> const& cubemap_textures);
		[[nodiscard]] GfxDescriptor GetSRV(TextureHandle handle);
		[[nodiscard]] GfxTexture* GetTexture(TextureHandle handle) const;
		void EnableMipMaps(bool);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx = nullptr;
		
		std::unordered_map<TextureName, TextureHandle> loaded_textures;
		std::unordered_map<TextureHandle, std::unique_ptr<GfxTexture>> texture_map;
		std::unordered_map<TextureHandle, GfxDescriptor> texture_srv_map;
		TextureHandle handle = 0;
		bool mipmaps = true;
		bool is_scene_initialized = false;

	private:
		TextureManager();
		~TextureManager();

		void CreateViewForTexture(TextureHandle handle, bool flag = false);
	};
	#define g_TextureManager TextureManager::Get()

}