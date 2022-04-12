
//https://github.com/chris-wyman/GettingStartedWithRTXRayTracing/blob/master/05-AmbientOcclusion/Data/Tutorial05/hlslUtils.hlsli
// Generates a seed for a random number generator from 2 inputs plus a backoff
static uint InitRand(uint val0, uint val1, uint backoff = 16)
{
    uint v0 = val0, v1 = val1, s0 = 0;

	[unroll]
    for (uint n = 0; n < backoff; n++)
    {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}
// Takes our seed, updates it, and returns a pseudorandom float in [0..1]
static float NextRand(inout uint s)
{
    s = (1664525u * s + 1013904223u);
    return float(s & 0x00FFFFFF) / float(0x01000000);
}
// Utility function to get a vector perpendicular to an input vector 
//    (from "Efficient Construction of Perpendicular Vectors Without Branching")
static float3 GetPerpendicularVector(float3 u)
{
    float3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, float3(xm, ym, zm));
}
// Get a cosine-weighted random vector centered around a specified normal direction.
static float3 GetCosHemisphereSample(inout uint randSeed, float3 hitNorm)
{
	// Get 2 random numbers to select our sample with
    float2 randVal = float2(NextRand(randSeed), NextRand(randSeed));

	// Cosine weighted hemisphere sample from RNG
    float3 bitangent = GetPerpendicularVector(hitNorm);
    float3 tangent = cross(bitangent, hitNorm);
    float r = sqrt(randVal.x);
    float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
    return tangent * (r * cos(phi).x) + bitangent * (r * sin(phi)) + hitNorm.xyz * sqrt(1 - randVal.x);
}
//https://medium.com/@alexander.wester/ray-tracing-soft-shadows-in-real-time-a53b836d123b
// Rotation with angle (in radians) and axis
static float3x3 AngleAxis3x3(float angle, float3 axis)
{
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
static float3 GetConeSample(inout uint randSeed, float3 direction, float coneAngle)
{
    float cosAngle = cos(coneAngle);

    float z = NextRand(randSeed) * (1.0f - cosAngle) + cosAngle;
    float phi = NextRand(randSeed) * 2.0f * 3.141592653589793238f;

    float x = sqrt(1.0f - z * z) * cos(phi);
    float y = sqrt(1.0f - z * z) * sin(phi);
    float3 north = float3(0.f, 0.f, 1.f);

    // Find the rotation axis `u` and rotation angle `rot` [1]
    float3 axis = normalize(cross(north, normalize(direction)));
    float angle = acos(dot(normalize(direction), north));

    // Convert rotation axis and angle to 3x3 rotation matrix [2]
    float3x3 R = AngleAxis3x3(angle, axis);

    return mul(R, float3(x, y, z));
}
/* A Fast and Robust Method for Avoiding
Self-Intersection by Carsten Wächter and Nikolaus Binder
Chapter 6. Ray Tracing Gems NVIDIA */
static float3 OffsetRay(const float3 p, const float3 n)
{
    static float origin = 1.0f / 32.0f;
    static float float_scale = 1.0f / 65536.0f;
    static float int_scale = 256.0f;

    int3 of_i = int3(int_scale * n.x, int_scale * n.y, int_scale * n.z);
    float3 p_i = float3(
                    asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
                    asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
                    asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return float3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
                  abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
                  abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

static const float M_PI = 3.141592654f;
static const float M_PI_2 = 6.283185307f;
static const float M_Pi_DIV_2 = 1.570796327f;
static const float M_Pi_DIV_4 = 0.7853981635f;
static const float M_PI_INV = 0.318309886f;
static const float FLT_MAX = 3.402823466E+38F;
static const float FLT_EPSILON = 1.19209290E-07F;

static float DegreesToRadians(float degrees)
{
    return degrees * (M_PI / 180.0f);
}

typedef BuiltInTriangleIntersectionAttributes HitAttributes;
