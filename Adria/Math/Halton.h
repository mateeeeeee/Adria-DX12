#pragma once

namespace adria
{
	struct Halton
	{
		static constexpr int FloorConstExpr(const Float val)
		{
			const auto val_int = (Sint64)val;
			const Float fval_int = (Float)val_int;
			return (int)(val >= (Float)0 ? fval_int : (val == fval_int ? val : fval_int - (Float)1));
		}
		constexpr Float operator()(int index, int base) const
		{
			Float f = 1;
			Float r = 0;
			while (index > 0)
			{
				f = f / base;
				r = r + f * (index % base);
				index = FloorConstExpr((Float)index / base);
			}
			return r;
		}
	};

	template<Uint32 SIZE, Uint32 BASE>
	struct HaltonSequence
	{
		constexpr HaltonSequence() : sequence{}
		{
			constexpr Halton generator;
			for (Uint32 i = 0; i < SIZE; ++i) sequence[i] = generator(i + 1, BASE);
		}

		constexpr Float operator[](Sint32 index) const
		{
			return sequence[index % SIZE];
		}

	private:

		Float sequence[SIZE];
	};
}