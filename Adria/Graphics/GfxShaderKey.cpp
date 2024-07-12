#include "GfxShaderKey.h"
#include "GfxShader.h"
#include "Rendering/ShaderManager.h"
#include "Utilities/HashUtil.h"

namespace adria
{
	struct GfxShaderKey::Impl
	{
		ShaderID id = ShaderID_Invalid;
		std::vector<GfxShaderDefine> defines;
	};

	GfxShaderKey::GfxShaderKey()
	{
		impl = std::make_unique<Impl>();
	}

	GfxShaderKey::GfxShaderKey(ShaderID shader_id) : GfxShaderKey()
	{
		impl->id = shader_id;
	}

	GfxShaderKey::GfxShaderKey(GfxShaderKey const& k) 
	{
		impl = std::make_unique<Impl>();
		impl->id = k.impl->id;
		impl->defines = k.impl->defines;
	}

	GfxShaderKey::~GfxShaderKey() = default;

	void GfxShaderKey::Init(ShaderID shader_id)
	{
		impl->id = shader_id;
	}

	GfxShaderKey& GfxShaderKey::operator=(GfxShaderKey const& k)
	{
		impl->id = k.impl->id;
		impl->defines = k.impl->defines;
		return *this;
	}

	void GfxShaderKey::operator=(ShaderID shader_id)
	{
		Init(shader_id);
	}

	void GfxShaderKey::AddDefine(char const* name, char const* value)
	{
		impl->defines.emplace_back(name, value);
	}

	bool GfxShaderKey::IsValid() const
	{
		return impl->id != ShaderID_Invalid;
	}

	GfxShaderKey::operator ShaderID() const
	{
		return impl->id;
	}

	std::vector<GfxShaderDefine> const& GfxShaderKey::GetDefines() const
	{
		return impl->defines;
	}

	ShaderID GfxShaderKey::GetShaderID() const
	{
		return impl->id;
	}

	uint64 GfxShaderKey::GetHash() const
	{
		std::string define_key;
		for (GfxShaderDefine const& define : impl->defines)
		{
			define_key += define.name;
			define_key += define.value;
		}
		define_key += std::to_string(impl->id);
		uint64 define_hash = crc64(define_key.c_str(), define_key.size());
		return define_hash;
	}

	bool GfxShaderKey::operator==(GfxShaderKey const& key) const
	{
		return GetHash() == key.GetHash();
	}

}
