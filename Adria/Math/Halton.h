#pragma once

namespace adria
{
	struct Halton
	{
		static constexpr Int FloorConstExpr(Float val)
		{
			const auto val_int = (Int64)val;
			const Float fval_int = (Float)val_int;
			return (Int)(val >= (Float)0 ? fval_int : (val == fval_int ? val : fval_int - (Float)1));
		}
		constexpr Float operator()(Int index, Int base) const
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

	template<Uint32 Size, Uint32 Base>
	struct HaltonSequence
	{
		constexpr HaltonSequence() : sequence{}
		{
			constexpr Halton generator;
			for (Uint32 i = 0; i < Size; ++i) sequence[i] = generator(i + 1, Base);
		}

		constexpr Float operator[](Uint index) const
		{
			return sequence[index % Size];
		}

	private:

		Float sequence[Size];
	};
}