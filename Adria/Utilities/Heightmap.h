#pragma once
#include <vector>
#include <string_view>

namespace adria
{
	enum class NoiseType
	{
		OpenSimplex2,
		OpenSimplex2S,
		Cellular,
		Perlin,
		ValueCubic,
		Value
	};
	enum class FractalType
	{
		None,
		FBM,
		Ridged,
		PingPong
	};
	struct NoiseDesc
	{
		Uint32 width;
		Uint32 depth;
		Uint32 max_height;
		FractalType fractal_type = FractalType::None;
		NoiseType noise_type = NoiseType::Perlin;
		Sint32 seed = 1337;
		float persistence = 0.5f;
		float lacunarity = 2.0f;
		Sint32 octaves = 3;
		float noise_scale = 10;
	};

	
	class Heightmap
	{
	public:	
		explicit Heightmap(NoiseDesc const& desc);
		explicit Heightmap(std::string_view heightmap_path);

		float HeightAt(Uint64 x, Uint64 z);
		Uint64 Width() const;
		Uint64 Depth() const;

	private:
		std::vector<std::vector<float>> hm;
	};
}