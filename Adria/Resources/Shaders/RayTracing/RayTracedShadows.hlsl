#include "LightInfo.hlsli"
#include "RayTracingUtil.hlsli"

struct RayTracedShadowsConstants
{
	uint  depthIdx;
	uint  lightIdx;
};
ConstantBuffer<RayTracedShadowsConstants> RayTracedShadowsPassCB : register(b1);

struct [raypayload] ShadowRayData
{
	bool hit : read(caller) : write(caller, miss, anyhit);
};

[shader("raygeneration")]
void RTS_RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	Texture2D<float> depthTexture = ResourceDescriptorHeap[RayTracedShadowsPassCB.depthIdx];
	
	LightInfo lightInfo = LoadLightInfo(RayTracedShadowsPassCB.lightIdx);
	RWTexture2D<float> outputTexture = ResourceDescriptorHeap[lightInfo.shadowMaskIndex];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float depth = depthTexture.Load(int3(launchIndex.xy, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.renderResolution;
	float3 worldPos = GetWorldPosition(texCoords, depth);

	lightInfo.direction.xyz = mul(lightInfo.direction.xyz, (float3x3) FrameCB.inverseView);
	lightInfo.position = mul(float4(lightInfo.position.xyz, 1.0f), FrameCB.inverseView);
	lightInfo.position.xyz /= lightInfo.position.w;

	float3 direction;
    float maxT;
	switch (lightInfo.type)
	{
	case DIRECTIONAL_LIGHT:
		direction = -lightInfo.direction.xyz;
		maxT = FLT_MAX;
		break;
	case POINT_LIGHT:
		direction = lightInfo.position.xyz - worldPos;
		maxT = length(direction);
		break;
	case SPOT_LIGHT:
		direction = -lightInfo.direction.xyz;
		maxT = length(lightInfo.position.xyz - worldPos);
		break;
	}

	RayDesc ray;
	ray.Origin = worldPos.xyz;
	ray.Direction = normalize(direction);
	ray.TMin = 1e-1f;
	ray.TMax = maxT;

	ShadowRayData payload;
	payload.hit = true;
	TraceRay(tlas, (RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES),
		0xFF, 0, 0, 0, ray, payload);
    outputTexture[launchIndex.xy] = payload.hit ? 0.0f : 1.0f;
}

[shader("miss")]
void RTS_Miss(inout ShadowRayData hitData)
{
	hitData.hit = false;
}


[shader("anyhit")]
void RTS_AnyHit(inout ShadowRayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
	hitData.hit = true;
}








