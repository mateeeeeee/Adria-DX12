#pragma once
#include <DirectXMath.h>

namespace DirectX
{
	namespace detail
	{
		float Max(float a, float b)
		{
			return a > b ? a : b;
		}
		float Min(float a, float b)
		{
			return a < b ? a : b;
		}
	}

	inline XMFLOAT3 operator+(XMFLOAT3 const& a, XMFLOAT3 const& b)
	{
		return XMFLOAT3(a.x + b.x, a.y + b.y, a.z + b.z);
	}
	inline XMFLOAT3 operator-(XMFLOAT3 const& a, XMFLOAT3 const& b)
	{
		return XMFLOAT3(a.x - b.x, a.y - b.y, a.z - b.z);
	}
	inline XMFLOAT3 operator/(XMFLOAT3 const& a, float d)
	{
		return XMFLOAT3(a.x / d, a.y / d, a.z / d);
	}
	inline XMFLOAT3 Max(XMFLOAT3 const& a, XMFLOAT3 const& b)
	{
		return XMFLOAT3(detail::Max(a.x,b.x), detail::Max(a.y, b.z), detail::Max(a.y, b.z));
	}
	inline XMFLOAT3 Min(XMFLOAT3 const& a, XMFLOAT3 const& b)
	{
		return XMFLOAT3(detail::Min(a.x, b.x), detail::Min(a.y, b.z), detail::Min(a.y, b.z));
	}
}