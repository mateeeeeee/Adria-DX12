#pragma once

namespace adria
{
	enum ShaderID : Uint8;
	struct GfxShaderDefine;

	class GfxShaderKey
	{
		struct Impl;
		friend class ShaderManager;
		friend struct GfxShaderKeyHash;
	public:
		GfxShaderKey();
		GfxShaderKey(ShaderID shader_id);
		~GfxShaderKey();

		GfxShaderKey(GfxShaderKey const&);
		GfxShaderKey& operator=(GfxShaderKey const&);

		void Init(ShaderID shader_id);
		void operator=(ShaderID shader_id);

		void AddDefine(Char const* name, Char const* value = "");
		Bool IsValid() const;

		std::vector<GfxShaderDefine> const& GetDefines() const;
		ShaderID GetShaderID() const;

		operator ShaderID() const;
		Bool operator==(GfxShaderKey const& key) const;

	private:
		std::unique_ptr<Impl> impl;

	private:
		Uint64 GetHash() const;
	};

	struct GfxShaderKeyHash
	{
		Uint64 operator()(GfxShaderKey const& key) const
		{
			return key.GetHash();
		}
	};
	#define GfxShaderKeyDefine(key, name, ...) key.AddDefine(ADRIA_STRINGIFY(name) __VA_OPT__(,) ADRIA_STRINGIFY(__VA_ARGS__))

}

