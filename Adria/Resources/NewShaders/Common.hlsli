#pragma once

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

uint GetCubeFaceIndex(float3 v)
{
    float3 vAbs = abs(v);
    uint faceIndex = 0;
    if (vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
    {
        faceIndex = v.z < 0 ? 5 : 4;
    }
    else if (vAbs.y >= vAbs.x)
    {
        faceIndex = v.y < 0 ? 3 : 2;
    }
    else
    {
        faceIndex = v.x < 0 ? 1 : 0;
    }
    return faceIndex;
}

static bool IsSaturated(float value)
{
    return value == saturate(value);
}

static bool IsSaturated(float2 value)
{
    return IsSaturated(value.x) && IsSaturated(value.y);
}

static bool IsSaturated(float3 value)
{
    return IsSaturated(value.x) && IsSaturated(value.y) && IsSaturated(value.z);
}

static bool IsSaturated(float4 value)
{
    return IsSaturated(value.x) && IsSaturated(value.y) && IsSaturated(value.z) && IsSaturated(value.w);
}

