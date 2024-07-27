#pragma once

namespace adria
{
	using TextureHandle = uint64;
	inline constexpr TextureHandle INVALID_TEXTURE_HANDLE = TextureHandle(-1);
	inline constexpr TextureHandle DEFAULT_BLACK_TEXTURE_HANDLE = TextureHandle(0);
	inline constexpr TextureHandle DEFAULT_WHITE_TEXTURE_HANDLE = TextureHandle(1);
	inline constexpr TextureHandle DEFAULT_NORMAL_TEXTURE_HANDLE = TextureHandle(2);
	inline constexpr TextureHandle DEFAULT_METALLIC_ROUGHNESS_TEXTURE_HANDLE = TextureHandle(3);
	inline constexpr TextureHandle TEXTURE_MANAGER_START_HANDLE = TextureHandle(4);
}