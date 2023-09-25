#pragma once

namespace adria
{
	struct SimpleVertex
	{
		Vector3 position;
	};

	struct TexturedVertex
	{
		Vector3 position;
		Vector2 uv;
	};

	struct TexturedNormalVertex
	{
		Vector3 position;
		Vector2 uv;
		Vector3 normal;
	};

	struct ColoredVertex
	{
		Vector3 position;
		Vector4 color;
	};

	struct ColoredNormalVertex
	{
		Vector3 position;
		Vector4 color;
		Vector3 normal;
	};

	struct NormalVertex
	{
		Vector3 position;
		Vector3 normal;
	};

	struct CompleteVertex
	{
		Vector3 position;
		Vector2 uv;
		Vector3 normal;
		Vector4 tangent;
	};

}

