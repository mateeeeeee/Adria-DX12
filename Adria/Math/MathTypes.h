#pragma once
#define __d3d12_h__
#include "SimpleMath.h"

namespace adria
{
	using BoundingBox = DirectX::BoundingBox;
	using OrientedBoundingBox = DirectX::BoundingOrientedBox;
	using BoundingFrustum = DirectX::BoundingFrustum;
	using BoundingSphere = DirectX::BoundingSphere;
	using Vector2 = DirectX::SimpleMath::Vector2;
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Vector4 = DirectX::SimpleMath::Vector4;
	using Matrix = DirectX::SimpleMath::Matrix;
	using Quaternion = DirectX::SimpleMath::Quaternion;
	using Color = DirectX::SimpleMath::Color;
	using Ray = DirectX::SimpleMath::Ray;
}