#include "CommonResources.hlsli"
#include "RayTracingUtil.hlsli"

struct RayTracedAmbientOcclusionConstants
{
	uint  accelStructIdx;
	uint  depthIdx;
	uint  outputIdx;
	uint  lightIdx;
};
ConstantBuffer<RayTracedAmbientOcclusionConstants> PassCB : register(b1);

struct ShadowRayData
{
	bool hit;
};

[shader("raygeneration")]
void RTS_RayGen()
{
	RaytracingAccelerationStructure scene = ResourceDescriptorHeap[PassCB.accelStructIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	RWTexture2D<float> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float depth = depthTx.Load(int3(launchIndex.xy, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.screenResolution;

	float3 worldPos = GetWorldPosition(texCoords, depth);

	Light light = lights[PassCB.lightIdx];
	float3 direction;
	float maxT;

	light.direction.xyz = mul(light.direction.xyz, (float3x3) FrameCB.inverseView);
	light.position = mul(float4(light.position.xyz, 1.0f), FrameCB.inverseView);
	light.position.xyz /= light.position.w;

	float softness = 0.0f;
	switch (light.type)
	{
	case POINT_LIGHT:
		direction = light.position.xyz - worldPos;
		maxT = length(direction);
		break;
	case DIRECTIONAL_LIGHT:
		direction = -light.direction.xyz;
		maxT = 1e9;
		softness = 0.5f;
		break;
	case SPOT_LIGHT:
		direction = -light.direction.xyz;
		maxT = length(light.position.xyz - worldPos);
		break;
	}

#ifndef SOFT_SHADOWS
	RayDesc ray;
	ray.Origin = worldPos.xyz;
	ray.Direction = normalize(direction);
	ray.TMin = 0.2f;
	ray.TMax = maxT;

	ShadowRayData payload;
	payload.hit = true;
	TraceRay(rt_scene, (RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES),
		0xFF, 0, 0, 0, ray, payload);
	outputTx[launchIndex.xy] = payload.hit ? 0.0f : 1.0f;

#else
	static const int RAY_COUNT = 1;

	uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, FrameCB.frameCount, 16); //0; set to 0 for deterministic output
	float shadowFactor = 0.0f;

	[unroll(RAY_COUNT)]
	for (int i = 0; i < RAY_COUNT; i++)
	{
		RayDesc ray;
		ray.Origin = posWorld.xyz;
		ray.Direction = normalize(GetConeSample(randSeed, direction, DegreesToRadians(softness)));
		ray.TMin = 0.2f;
		ray.TMax = maxT;

		ShadowRayData payload;
		payload.hit = true;
		TraceRay(rt_scene, (RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES),
			0xFF, 0, 0, 0, ray, payload);
		shadowFactor += payload.hit ? 0.0f : 1.0f;
	}
	outputTx[launchIndex.xy] = shadowFactor / RAY_COUNT;
#endif
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








