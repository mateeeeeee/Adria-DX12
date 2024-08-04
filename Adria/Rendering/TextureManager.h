#pragma once
#include "TextureHandle.h"
#include "Graphics/GfxDescriptor.h"
#include "Utilities/Singleton.h"
#include "Utilities/Ref.h"

namespace adria
{
	class GfxDevice;
	class GfxTexture;

	class TextureManager : public Singleton<TextureManager>
	{
		friend class Singleton<TextureManager>;
		using TextureName = std::string;

	public:

		void Initialize(GfxDevice* gfx, uint32 max_textures);
		void Destroy();

		ADRIA_NODISCARD TextureHandle LoadTexture(std::string_view path);
		ADRIA_NODISCARD TextureHandle LoadCubemap(std::array<std::string, 6> const& cubemap_textures);
		ADRIA_NODISCARD GfxDescriptor GetSRV(TextureHandle handle);
		ADRIA_NODISCARD GfxTexture* GetTexture(TextureHandle handle) const;
		void EnableMipMaps(bool);
		void OnSceneInitialized();

	private:
		GfxDevice* gfx = nullptr;
		
		std::unordered_map<TextureName, TextureHandle> loaded_textures;
		std::unordered_map<TextureHandle, std::unique_ptr<GfxTexture>> texture_map;
		std::unordered_map<TextureHandle, GfxDescriptor> texture_srv_map;
		TextureHandle handle = TEXTURE_MANAGER_START_HANDLE;
		bool mipmaps = true;
		bool is_scene_initialized = false;

	private:
		TextureManager();
		~TextureManager();

		void CreateViewForTexture(TextureHandle handle, bool flag = false);
	};
	#define g_TextureManager TextureManager::Get()

}