#ifndef _COMMON_RESOURCES_
#define _COMMON_RESOURCES_

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
	row_major matrix rainViewProjection;
	float4 cameraPosition;
	float4 cameraForward;
	float2 cameraJitter;
	float  cameraNear;
	float  cameraFar;

	float4 ambientColor;
    float4 windParams;
    float4 sunDirection;
    float4 sunColor;
	float4 cascadeSplits;

	float2 displayResolution;
	float2 renderResolution;

    float2 mouseNormalizedCoords;
	float  deltaTime;
	float  totalTime;

	uint   frameCount;
	int    lightsIdx;
	int    lightCount;
	int    lightsMatricesIdx;
    int	   accelStructIdx;
    int	   envMapIdx;
	int    meshesIdx;
	int    materialsIdx;
	int    instancesIdx;
	int    ddgiVolumesIdx;
	int    printfBufferIdx;

	int	   rainSplashDiffuseIdx;
	int	   rainSplashBumpIdx;
	int	   rainBlockerMapIdx;
	int    sheenEIdx;
	int    triangleOverdrawIdx;
	float  rainTotalTime;
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

SamplerState LinearMirrorSampler : register(s8);
SamplerState PointMirrorSampler : register(s9);


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

static float DelinearizeDepth(float linearZ)
{
    return (linearZ * FrameCB.cameraFar - FrameCB.cameraNear * FrameCB.cameraFar) / (linearZ * (FrameCB.cameraFar - FrameCB.cameraNear));
}

static float LinearizeDepth(float z)
{
    return FrameCB.cameraNear * FrameCB.cameraFar / (FrameCB.cameraFar - z * (FrameCB.cameraFar - FrameCB.cameraNear));
}

static uint2 FullScreenPosition(uint2 halfScreenPos)
{
    static const uint2 offsets[4] =
    {
        uint2(1, 1),
        uint2(1, 0),
        uint2(0, 0),
        uint2(0, 1),
    };
    return halfScreenPos * 2 + offsets[FrameCB.frameCount % 4];
}

float3 GetWorldPosition(uint2 screenPos, float depth)
{
	float2 screenUV = ((float2)screenPos + 0.5) * rcp(FrameCB.renderResolution);
    return GetWorldPosition(screenUV, depth);
}

float GetCameraAspectRatio()
{
    float fx = FrameCB.projection._11; 
    float fy = FrameCB.projection._22; 
    return fx / fy;
}

float3 UintToColor(uint id)
{
    id = (id ^ 61) ^ (id >> 16);
    id = id + (id << 3);
    id = id ^ (id >> 4);
    id = id * 0x27d4eb2d;
    id = id ^ (id >> 15);
    
    float r = float((id & 0xFF)) / 255.0f;
    float g = float((id >> 8) & 0xFF) / 255.0f;
    float b = float((id >> 16) & 0xFF) / 255.0f;
    
    r = lerp(0.2f, 1.0f, r);
    g = lerp(0.2f, 1.0f, g);
    b = lerp(0.2f, 1.0f, b);
    
    return float3(r, g, b);
}

#endif