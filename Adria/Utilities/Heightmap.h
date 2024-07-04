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
		uint32 width;
		uint32 depth;
		uint32 max_height;
		FractalType fractal_type = FractalType::None;
		NoiseType noise_type = NoiseType::Perlin;
		int32 seed = 1337;
		float persistence = 0.5f;
		float lacunarity = 2.0f;
		int32 octaves = 3;
		float noise_scale = 10;
	};

	
	class Heightmap
	{
	public:	
		explicit Heightmap(NoiseDesc const& desc);
		explicit Heightmap(std::string_view heightmap_path);

		float HeightAt(uint64 x, uint64 z);
		uint64 Width() const;
		uint64 Depth() const;

	private:
		std::vector<std::vector<float>> hm;
	};
}