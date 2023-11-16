#pragma once

namespace adria
{
	struct Halton
	{
		static constexpr int FloorConstExpr(const float val)
		{
			const auto val_int = (int64)val;
			const float fval_int = (float)val_int;
			return (int)(val >= (float)0 ? fval_int : (val == fval_int ? val : fval_int - (float)1));
		}
		constexpr float operator()(int index, int base) const
		{
			float f = 1;
			float r = 0;
			while (index > 0)
			{
				f = f / base;
				r = r + f * (index % base);
				index = FloorConstExpr((float)index / base);
			}
			return r;
		}
	};

	template<uint32 SIZE, uint32 BASE>
	struct HaltonSequence
	{
		constexpr HaltonSequence() : sequence{}
		{
			constexpr Halton generator;
			for (uint32 i = 0; i < SIZE; ++i) sequence[i] = generator(i + 1, BASE);
		}

		constexpr float operator[](int32 index) const
		{
			return sequence[index % SIZE];
		}

	private:

		float sequence[SIZE];
	};
}