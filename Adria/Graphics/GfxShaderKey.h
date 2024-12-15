#pragma once

namespace adria
{
	enum ShaderID : Uint8;
	struct GfxShaderDefine;

	class GfxShaderKey
	{
		struct Impl;
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
		Uint64 GetHash() const;

		operator ShaderID() const;
		Bool operator==(GfxShaderKey const& key) const;

	private:
		std::unique_ptr<Impl> impl;
	};

	struct GfxShaderKeyHash
	{
		Uint64 operator()(GfxShaderKey const& key) const
		{
			return key.GetHash();
		}
	};
}

