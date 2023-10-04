#ifndef COMMON_RESOURCES_INCLUDED
#define COMMON_RESOURCES_INCLUDED

struct FrameCBuffer
{
	row_major matrix view;
	row_major matrix projection;
	row_major matrix viewProjection;
	row_major matrix inverseView;
	row_major matrix inverseProjection;
	row_major matrix inverseViewProjection;
	row_major matrix prevView;
	row_major matrix prevProjection;
	row_major matrix prevViewProjection;
	row_major matrix reprojection;
	float4 cameraPosition;
	float4 cameraForward;
	float2 cameraJitter;
	float  cameraNear;
	float  cameraFar;

    float4 windParams;
    float4 sunDirection;
    float4 sunColor;
	float4 cascadeSplits;

	float2 screenResolution;
    float2 mouseNormalizedCoords;

	float  deltaTime;
	float  totalTime;
	uint   frameCount;
	int    lightsIdx;
	int    lightsMatricesIdx;
    int	   accelStructIdx;
    int	   envMapIdx;
	int    meshesIdx;
	int    materialsIdx;
	int    instancesIdx;
	int    ddgiVolumesIdx;
};
ConstantBuffer<FrameCBuffer> FrameCB  : register(b0);

SamplerState LinearWrapSampler : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerState LinearBorderSampler : register(s2);

SamplerState PointWrapSampler : register(s3);
SamplerState PointClampSampler : register(s4);
SamplerState PointBorderSampler : register(s5);

SamplerComparisonState ShadowClampSampler : register(s6);
SamplerComparisonState ShadowWrapSampler : register(s7);

static float3 GetViewPosition(float2 texcoord, float depth)
{
	float4 clipSpaceLocation;
	clipSpaceLocation.xy = texcoord * 2.0f - 1.0f;
	clipSpaceLocation.y *= -1;
	clipSpaceLocation.z = depth;
	clipSpaceLocation.w = 1.0f;
	float4 homogenousLocation = mul(clipSpaceLocation, FrameCB.inverseProjection);
	return homogenousLocation.xyz / homogenousLocation.w;
}

static float3 GetWorldPosition(float2 texcoord, float depth)
{
	float4 clipSpaceLocation;
	clipSpaceLocation.xy = texcoord * 2.0f - 1.0f;
	clipSpaceLocation.y *= -1;
	clipSpaceLocation.z = depth;
	clipSpaceLocation.w = 1.0f;
	float4 worldLocation = mul(clipSpaceLocation, FrameCB.inverseViewProjection);
	return worldLocation.xyz / worldLocation.w;
}

static float3 ClipToView(float4 clip)
{
	float4 view = mul(clip, FrameCB.inverseProjection);
	view = view / view.w;
	return view.xyz;
}

static float3 ViewToWindow(float3 view)
{
	float4 proj = mul(float4(view, 1), FrameCB.projection);
	proj.xyz /= proj.w;
	proj.x = (proj.x + 1) / 2;
	proj.y = 1 - (proj.y + 1) / 2;
	return proj.xyz;
}

static float LinearizeDepth(float z)
{
	return FrameCB.cameraFar / (FrameCB.cameraFar + z * (FrameCB.cameraNear - FrameCB.cameraFar));
}

#endif