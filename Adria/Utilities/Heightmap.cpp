#include "Heightmap.h"
//move to cpp
#include "Cpp/FastNoiseLite.h"

namespace adria
{
	
	constexpr FastNoiseLite::NoiseType GetNoiseType(ENoiseType type)
	{
		switch (type)
		{
		case ENoiseType::OpenSimplex2:
			return FastNoiseLite::NoiseType_OpenSimplex2;
		case ENoiseType::OpenSimplex2S:
			return FastNoiseLite::NoiseType_OpenSimplex2S;
		case ENoiseType::Cellular:
			return FastNoiseLite::NoiseType_OpenSimplex2;
		case ENoiseType::ValueCubic:
			return FastNoiseLite::NoiseType_ValueCubic;
		case ENoiseType::Value:
			return FastNoiseLite::NoiseType_Value;
		case ENoiseType::Perlin:
		default:
			return FastNoiseLite::NoiseType_Perlin;
		}

		return FastNoiseLite::NoiseType_Perlin;
	}
	constexpr FastNoiseLite::FractalType GetFractalType(EFractalType type)
	{
		switch (type)
		{
		case EFractalType::FBM:
			return FastNoiseLite::FractalType_FBm;
		case EFractalType::Ridged:
			return FastNoiseLite::FractalType_Ridged;
		case EFractalType::PingPong:
			return FastNoiseLite::FractalType_PingPong;
		case EFractalType::None:
		default:
			return FastNoiseLite::FractalType_None;
		}

		return FastNoiseLite::FractalType_None;
	}


	Heightmap::Heightmap(noise_desc_t const& desc)
	{
		FastNoiseLite noise{};
		noise.SetFractalType(GetFractalType(desc.fractal_type));
		noise.SetSeed(desc.seed);
		noise.SetNoiseType(GetNoiseType(desc.noise_type));
		noise.SetFractalOctaves(desc.octaves);
		noise.SetFractalLacunarity(desc.lacunarity);
		noise.SetFractalGain(desc.persistence);
		noise.SetFrequency(0.1f);
		hm.resize(desc.depth);

		for (uint32 z = 0; z < desc.depth; z++)
		{
			hm[z].resize(desc.width);
			for (uint32 x = 0; x < desc.width; x++)
			{

				float32 xf = x * desc.noise_scale / desc.width; // - desc.width / 2;
				float32 zf = z * desc.noise_scale / desc.depth; // - desc.depth / 2;

				float32 total = noise.GetNoise(xf, zf);

				hm[z][x] = total * desc.max_height;
			}
		}
	}
	Heightmap::Heightmap(std::string_view heightmap_path)
	{
		//todo
	}
	float32 Heightmap::HeightAt(uint64 x, uint64 z)
	{
		return hm[z][x];
	}
	uint64 Heightmap::Width() const
	{
		return hm[0].size();
	}
	uint64 Heightmap::Depth() const
	{
		return hm.size();
	}
}