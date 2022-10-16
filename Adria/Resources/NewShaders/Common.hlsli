static const float M_PI = 3.141592654f;
static const float M_PI_2 = 6.283185307f;
static const float M_PI_DIV_2 = 1.570796327f;
static const float M_PI_DIV_4 = 0.7853981635f;
static const float M_PI_INV = 0.318309886f;
static const float FLT_MAX = 3.402823466E+38F;
static const float FLT_EPSILON = 1.19209290E-07F;

static float DegreesToRadians(float degrees)
{
	return degrees * (M_PI / 180.0f);
}

static float3x3 AngleAxis3x3(float angle, float3 axis)
{
	// Rotation with angle (in radians) and axis
	float c, s;
	sincos(angle, s, c);

	float t = 1 - c;
	float x = axis.x;
	float y = axis.y;
	float z = axis.z;

	return float3x3(
		t * x * x + c, t * x * y - s * z, t * x * z + s * y,
		t * x * y + s * z, t * y * y + c, t * y * z - s * x,
		t * x * z - s * y, t * y * z + s * x, t * z * z + c
		);
}

static float3 GetPerpendicularVector(float3 u)
{
	// Utility function to get a vector perpendicular to an input vector 
	// (from "Efficient Construction of Perpendicular Vectors Without Branching")
	float3 a = abs(u);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(u, float3(xm, ym, zm));
}


