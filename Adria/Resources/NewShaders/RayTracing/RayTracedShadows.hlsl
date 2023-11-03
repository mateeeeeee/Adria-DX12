#include "../Lighting.hlsli"
#include "RayTracingUtil.hlsli"

struct RayTracedShadowsConstants
{
	uint  depthIdx;
	uint  lightIdx;
};
ConstantBuffer<RayTracedShadowsConstants> PassCB : register(b1);

struct ShadowRayData
{
	bool hit;
};

[shader("raygeneration")]
void RTS_RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	Light light = lights[PassCB.lightIdx];
	RWTexture2D<float> outputTx = ResourceDescriptorHeap[light.shadowMaskIndex];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float depth = depthTx.Load(int3(launchIndex.xy, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.renderResolution;
	float3 worldPos = GetWorldPosition(texCoords, depth);

	light.direction.xyz = mul(light.direction.xyz, (float3x3) FrameCB.inverseView);
	light.position = mul(float4(light.position.xyz, 1.0f), FrameCB.inverseView);
	light.position.xyz /= light.position.w;

	float3 direction;
    float maxT;
	switch (light.type)
	{
	case DIRECTIONAL_LIGHT:
		direction = -light.direction.xyz;
		maxT = FLT_MAX;
		break;
	case POINT_LIGHT:
		direction = light.position.xyz - worldPos;
		maxT = length(direction);
		break;
	case SPOT_LIGHT:
		direction = -light.direction.xyz;
		maxT = length(light.position.xyz - worldPos);
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
    outputTx[launchIndex.xy] = payload.hit ? 0.0f : 1.0f;
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








