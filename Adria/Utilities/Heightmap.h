#pragma once
#include "../Core/Definitions.h"
#include <vector>
#include <string_view>

namespace adria
{
	enum class ENoiseType
	{
		OpenSimplex2,
		OpenSimplex2S,
		Cellular,
		Perlin,
		ValueCubic,
		Value
	};
	enum class EFractalType
	{
		None,
		FBM,
		Ridged,
		PingPong
	};
	struct noise_desc_t
	{
		U32 width;
		U32 depth;
		U32 max_height;
		EFractalType fractal_type = EFractalType::None;
		ENoiseType noise_type = ENoiseType::Perlin;
		I32 seed = 1337;
		F32 persistence = 0.5f;
		F32 lacunarity = 2.0f;
		I32 octaves = 3;
		F32 noise_scale = 10;
	};

	
	class Heightmap
	{

	public:
		
		Heightmap(noise_desc_t const& desc);

		Heightmap(std::string_view heightmap_path);

		F32 HeightAt(U64 x, U64 z);

		U64 Width() const;

		U64 Depth() const;

	private:
		std::vector<std::vector<F32>> hm;
	};
}


/*
	enum CellularDistanceFunction
	{
		CellularDistanceFunction_Euclidean,
		CellularDistanceFunction_EuclideanSq,
		CellularDistanceFunction_Manhattan,
		CellularDistanceFunction_Hybrid
	};

	enum CellularReturnType
	{
		CellularReturnType_CellValue,
		CellularReturnType_Distance,
		CellularReturnType_Distance2,
		CellularReturnType_Distance2Add,
		CellularReturnType_Distance2Sub,
		CellularReturnType_Distance2Mul,
		CellularReturnType_Distance2Div
	};

	*/

/*struct THERMAL_EROSION_DESC
	{
		int iterations;
		float c;
		float talus;
	};

	struct HYDRAULIC_EROSION_DESC
	{
		int iterations;
		int drops;
		float carryingCapacity;
		float depositionSpeed;

	void ApplyThermalErosion(const THERMAL_EROSION_DESC&);

	void ApplyHydraulicErosion(const HYDRAULIC_EROSION_DESC&);

	floatDepositSediment(float c, float maxDiff, float talus, float distance, float totalDiff)
	{
		return (distance > talus) ? (c * (maxDiff - talus) * (distance / totalDiff)) : 0.0f;
		// Make sure that the distance is greater than the talus angle/threshold
	}
	};


*/

/*

//daj popravi ovaj nered
void Heightmap::ApplyThermalErosion(const THERMAL_EROSION_DESC& desc)
{
	int index;									// Index of the current vertex
	float v1, v2, v3, v4, v5, v6, v7, v8, v9;	// Vertex v2 and its Moore neighbourhood
	float d1, d2, d3, d4, d5, d6, d7, d8;		// Differences in height for each neighbour
	float talus = desc.talus;//4.0f / resolution;			// Calculate a reasonable talus angle
	float c = desc.c; //0.5f;								// Constant C

	for (unsigned int k = 0; k < desc.iterations; k++)
	{

		// Loop vertically
		for (unsigned long j = 0; j < hm.size(); j++)
		{

			// Loop horizontally
			for (unsigned long i = 0; i < hm[j].size(); i++)
			{

				// Re-initialise variables for each loop
				float maxDiff = 0.0f;
				float totalDiff = 0.0f;

				// Initialise height difference variables
				d1 = 0.0f;
				d2 = 0.0f;
				d3 = 0.0f;
				d4 = 0.0f;
				d5 = 0.0f;
				d6 = 0.0f;
				d7 = 0.0f;
				d8 = 0.0f;



				v2 = hm[j][i];

				//PRERUZNO ALI RADI
				if (i == 0 && j == 0)
				{

					v3 = hm[j][i + 1l];
					v5 = hm[j + 1l][i];
					v6 = hm[j + 1l][i + 1l];

					d2 = v2 - v3;
					d4 = v2 - v5;
					d5 = v2 - v6;

				}
				else if (i == hm[j].size() - 1 && j == hm.size() - 1)
				{

					v1 = hm[j][i - 1l];
					v7 = hm[j - 1l][i - 1l];
					v8 = hm[j - 1l][i];

					d1 = v2 - v1;
					d6 = v2 - v7;
					d7 = v2 - v8;

				}
				else if (i == 0 && j == hm.size() - 1)
				{

					v3 = hm[j][i + 1l];
					v8 = hm[j - 1l][i];
					v9 = hm[j - 1l][i + 1l];

					d2 = v2 - v3;
					d7 = v2 - v8;
					d8 = v2 - v9;

				}
				else if (i == hm[j].size() - 1 && j == 0)
				{

					v1 = hm[j][i - 1l];
					v4 = hm[j + 1l][i - 1l];
					v5 = hm[j + 1l][i];

					d1 = v2 - v1;
					d3 = v2 - v4;
					d4 = v2 - v5;

				}
				else if (j == 0)
				{

					v1 = hm[j][i - 1l];
					v3 = hm[j][i + 1l];
					v4 = hm[j + 1l][i - 1l];
					v5 = hm[j + 1l][i];
					v6 = hm[j + 1l][i + 1l];

					d1 = v2 - v1;
					d2 = v2 - v3;
					d3 = v2 - v4;
					d4 = v2 - v5;
					d5 = v2 - v6;

				}
				else if (j == hm.size() - 1)
				{

					v1 = hm[j][i - 1l];
					v3 = hm[j][i + 1l];
					v7 = hm[j - 1l][i - 1l];
					v8 = hm[j - 1l][i];
					v9 = hm[j - 1l][i + 1l];

					d1 = v2 - v1;
					d2 = v2 - v3;
					d6 = v2 - v7;
					d7 = v2 - v8;
					d8 = v2 - v9;

				}
				else if (i == 0)
				{

					v3 = hm[j][i + 1l];
					v5 = hm[j + 1l][i];
					v6 = hm[j + 1l][i + 1l];
					v8 = hm[j - 1l][i];
					v9 = hm[j - 1l][i + 1l];

					d2 = v2 - v3;
					d4 = v2 - v5;
					d5 = v2 - v6;
					d7 = v2 - v8;
					d8 = v2 - v9;

				}
				else if (i == hm[j].size() - 1)
				{

					v1 = hm[j][i - 1l];
					v4 = hm[j + 1l][i - 1l];
					v5 = hm[j + 1l][i];
					v7 = hm[j - 1l][i - 1l];
					v8 = hm[j - 1l][i];

					d1 = v2 - v1;
					d3 = v2 - v4;
					d4 = v2 - v5;
					d6 = v2 - v7;
					d7 = v2 - v8;

				}
				else
				{

					v1 = hm[j][i - 1l];
					v3 = hm[j][i + 1l];
					v4 = hm[j + 1l][i - 1l];
					v5 = hm[j + 1l][i];
					v6 = hm[j + 1l][i + 1l];
					v7 = hm[j - 1l][i - 1l];
					v8 = hm[j - 1l][i];
					v9 = hm[j - 1l][i + 1l];

					// Calculate the differences of the heights between each vertex
					d1 = v2 - v1;
					d2 = v2 - v3;
					d3 = v2 - v4;
					d4 = v2 - v5;
					d5 = v2 - v6;
					d6 = v2 - v7;
					d7 = v2 - v8;
					d8 = v2 - v9;

				}

				// Get the maximum height and total height
				if (d1 > talus) {

					totalDiff += d1;

					if (d1 > maxDiff) {

						maxDiff = d1;

					}

				}
				if (d2 > talus) {

					totalDiff += d2;

					if (d2 > maxDiff) {

						maxDiff = d2;

					}

				}

				if (d3 > talus) {

					totalDiff += d3;

					if (d3 > maxDiff) {

						maxDiff = d3;

					}

				}

				if (d4 > talus) {

					totalDiff += d4;

					if (d4 > maxDiff) {

						maxDiff = d4;

					}

				}

				if (d5 > talus) {

					totalDiff += d5;

					if (d5 > maxDiff) {

						maxDiff = d5;

					}

				}

				if (d6 > talus) {

					totalDiff += d6;

					if (d6 > maxDiff) {

						maxDiff = d6;

					}

				}

				if (d7 > talus) {

					totalDiff += d7;

					if (d7 > maxDiff) {

						maxDiff = d7;

					}

				}

				if (d8 > talus) {

					totalDiff += d8;

					if (d8 > maxDiff) {

						maxDiff = d8;

					}

				}

				// Assign new height values for relevant vertices 

				// First check corners for erosion so that we don't go off the edge of the heightmap
				if (i == 0 && j == 0)
				{

					hm[j][i + 1l] += DepositSediment(c, maxDiff, talus, d2, totalDiff);
					hm[j + 1l][i] += DepositSediment(c, maxDiff, talus, d4, totalDiff);
					hm[j + 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d5, totalDiff);

				}
				else if (i == hm[j].size() - 1 && j == hm.size() - 1)
				{

					hm[j][i - 1l] += DepositSediment(c, maxDiff, talus, d1, totalDiff);
					hm[j - 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d6, totalDiff);
					hm[j - 1l][i] += DepositSediment(c, maxDiff, talus, d7, totalDiff);

				}
				else if (i == 0 && j == hm.size() - 1)
				{

					hm[j][i + 1l] += DepositSediment(c, maxDiff, talus, d2, totalDiff);
					hm[j - 1l][i] += DepositSediment(c, maxDiff, talus, d7, totalDiff);
					hm[j - 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d8, totalDiff);

				}
				else if (i == hm[j].size() - 1 && j == 0)
				{

					hm[j][i - 1l] += DepositSediment(c, maxDiff, talus, d1, totalDiff);
					hm[j + 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d3, totalDiff);
					hm[j + 1l][i] += DepositSediment(c, maxDiff, talus, d4, totalDiff);

				}
				// Then check the sides for the same reason
				else if (j == 0)
				{

					hm[j][i - 1l] += DepositSediment(c, maxDiff, talus, d1, totalDiff);
					hm[j][i + 1l] += DepositSediment(c, maxDiff, talus, d2, totalDiff);
					hm[j + 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d3, totalDiff);
					hm[j + 1l][i] += DepositSediment(c, maxDiff, talus, d4, totalDiff);
					hm[j + 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d5, totalDiff);

				}
				else if (j == hm.size() - 1)
				{

					hm[j][i - 1l] += DepositSediment(c, maxDiff, talus, d1, totalDiff);
					hm[j][i + 1l] += DepositSediment(c, maxDiff, talus, d2, totalDiff);
					hm[j - 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d6, totalDiff);
					hm[j - 1l][i] += DepositSediment(c, maxDiff, talus, d7, totalDiff);
					hm[j - 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d8, totalDiff);

				}
				else if (i == 0)
				{

					hm[j][i + 1l] += DepositSediment(c, maxDiff, talus, d2, totalDiff);
					hm[j + 1l][i] += DepositSediment(c, maxDiff, talus, d4, totalDiff);
					hm[j + 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d5, totalDiff);
					hm[j - 1l][i] += DepositSediment(c, maxDiff, talus, d7, totalDiff);
					hm[j - 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d8, totalDiff);

				}
				else if (i == hm[j].size() - 1)
				{

					hm[j][i - 1l] += DepositSediment(c, maxDiff, talus, d1, totalDiff);
					hm[j + 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d3, totalDiff);
					hm[j + 1l][i] += DepositSediment(c, maxDiff, talus, d4, totalDiff);
					hm[j - 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d6, totalDiff);
					hm[j - 1l][i] += DepositSediment(c, maxDiff, talus, d7, totalDiff);

				}
				// Then finally do normal erosion for all other vertices
				else
				{

					hm[j][i - 1l] += DepositSediment(c, maxDiff, talus, d1, totalDiff);
					hm[j][i + 1l] += DepositSediment(c, maxDiff, talus, d2, totalDiff);
					hm[j + 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d3, totalDiff);
					hm[j + 1l][i] += DepositSediment(c, maxDiff, talus, d4, totalDiff);
					hm[j + 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d5, totalDiff);
					hm[j - 1l][i - 1l] += DepositSediment(c, maxDiff, talus, d6, totalDiff);
					hm[j - 1l][i] += DepositSediment(c, maxDiff, talus, d7, totalDiff);
					hm[j - 1l][i + 1l] += DepositSediment(c, maxDiff, talus, d8, totalDiff);

				}

			}

		}

	}
}

void Heightmap::ApplyHydraulicErosion(const HYDRAULIC_EROSION_DESC& desc)
{
	int drops = desc.drops;


	float xresolution = hm[0].size();
	float yresolution = hm.size();


	for (int drop = 0; drop < drops; drop++)
	{

		// Get random coordinates to drop the water droplet at
		int X = random(0, hm[0].size() - 1);
		int Y = random(0, hm.size() - 1);

		// Initialise working values
		float carryingAmount = 0.0f;	// Amount of sediment that is currently being carried
		float minSlope = 1.15f;			// Minimum value for the slope/height difference
					// Persistence

		// Limit calculations to only be run on terrain that is above water
		// This will speed up the calculations considerably
		if (hm[Y][X] > 0.0f)
		{

			// Iterate based on user's input
			for (int iter = 0; iter < desc.iterations; iter++)
			{

				// Get the location of the cell and its von Neumann neighbourhood
				float val = hm[Y][X];
				float left = 1000.0f;
				float right = 1000.0f;
				float up = 1000.0f;
				float down = 1000.0f;

				// Get the relevant vertices and distances for each vertex (need to do this to prevent read access violations)
				if (X == 0 && Y == 0)
				{

					right = hm[Y][X + 1];
					up = hm[Y + 1][X];

				}
				else if (X == xresolution - 1 && Y == yresolution - 1)
				{

					left = hm[Y][X - 1];
					down = hm[Y - 1][X];

				}
				else if (X == 0 && Y == yresolution - 1)
				{

					down = hm[Y - 1][X];
					right = hm[Y][X + 1];

				}
				else if (X == xresolution - 1 && Y == 0)
				{

					left = hm[Y][X - 1];
					up = hm[Y + 1][X];

				}
				else if (Y == 0)
				{

					left = hm[Y][X - 1];
					right = hm[Y][X + 1];
					up = hm[Y + 1][X];

				}
				else if (Y == yresolution - 1)
				{

					left = hm[Y][X - 1];
					right = hm[Y][X + 1];
					down = hm[Y - 1][X];

				}
				else if (X == 0)
				{

					right = hm[Y][X + 1];
					up = hm[Y + 1][X];
					down = hm[Y - 1][X];

				}
				else if (X == xresolution - 1)
				{

					left = hm[Y][X - 1];
					up = hm[Y + 1][X];
					down = hm[Y - 1][X];

				}
				else
				{

					left = hm[Y][X - 1];
					right = hm[Y][X + 1];
					up = hm[Y + 1][X];
					down = hm[Y - 1][X];

				}

				static const enum MIN_INDEX
				{
					CENTER = -1, LEFT, RIGHT, UP, DOWN
				};
				// Find the minimum height value among the cell's neighbourhood, and its location
				float minHeight = val;
				int minIndex = CENTER;

				if (left < minHeight)
				{

					minHeight = left;
					minIndex = LEFT;

				}

				if (right < minHeight)
				{

					minHeight = right;
					minIndex = RIGHT;

				}

				if (up < minHeight)
				{

					minHeight = up;
					minIndex = UP;

				}

				if (down < minHeight)
				{

					minHeight = down;
					minIndex = DOWN;

				}

				// If the lowest neighbor is NOT greater than the current value
				if (minHeight < val) {

					// Deposit or erode
					float slope = std::min(minSlope, (val - minHeight));
					float valueToSteal = desc.depositionSpeed * slope;

					// If carrying amount is greater than carryingCapacity
					if (carryingAmount > desc.carryingCapacity)
					{

						// Deposit sediment
						carryingAmount -= valueToSteal;
						hm[Y][X] += valueToSteal;

					}
					else {

						// Else erode the cell
						// Check that we're within carrying capacity
						if (carryingAmount + valueToSteal > desc.carryingCapacity)
						{

							// If not, calculate the amount that's above the carrying capacity and erode by delta
							float delta = carryingAmount + valueToSteal - desc.carryingCapacity;
							carryingAmount += delta;
							hm[Y][X] -= delta;

						}
						else
						{

							// Else erode by valueToSteal
							carryingAmount += valueToSteal;
							hm[Y][X] -= valueToSteal;

						}

					}

					// Move to next value
					if (minIndex == LEFT) {

						// Left
						X -= 1;

					}

					if (minIndex == RIGHT) {

						// Right
						X += 1;

					}

					if (minIndex == UP) {

						// Up
						Y += 1;

					}

					if (minIndex == DOWN) {

						// Down
						Y -= 1;

					}

					// Limiting to edge of map
					if (X > xresolution - 1) {

						X = xresolution - 1;

					}

					if (Y > yresolution - 1) {

						Y = yresolution - 1;

					}

					if (Y < 0) {

						Y = 0;

					}

					if (X < 0) {

						X = 0;

					}

				}

			}

		}



	}
}

*/