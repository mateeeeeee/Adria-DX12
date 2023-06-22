#include "DDGICommon.hlsli"
#include "../Lighting.hlsli"
#include "../Common.hlsli"
#include "../RayTracing/RayTracingUtil.hlsli"

struct DDGIRayTracePassConstants
{
	float3 randomVector;
	float  randomAngle;
	float  historyBlendWeight;
	uint   rayBufferIdx;
};
ConstantBuffer<DDGIRayTracePassConstants> PassCB : register(b1);
ConstantBuffer<DDGIVolume> DDGIVolumeCB			 : register(b2);

struct DDGIPayload
{
	float3 irradiance;
	float  distance;
};


[shader("raygeneration")]
void DDGI_RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];

	uint probeIdx = DispatchRaysIndex().y;
	uint rayIdx = DispatchRaysIndex().x;

	float3 probeLocation = GetProbeLocation(DDGIVolumeCB, probeIdx);
	float3x3 randomRotation = AngleAxis3x3(PassCB.randomAngle, PassCB.randomVector);
	float3  randomDirection = normalize(mul(SphericalFibonacci(rayIdx, DDGIVolumeCB.raysPerProbe), randomRotation));

	RayDesc ray;
	ray.Origin = probeLocation;
	ray.Direction = randomDirection;
	ray.TMin = 0.01f;
	ray.TMax = FLT_MAX;

	DDGIPayload payload;
	payload.irradiance = float3(0, 0, 0);
	payload.distance = FLT_MAX;
	TraceRay(tlas, (RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES), 0xFF, 0, 0, 0, ray, payload);
	RWBuffer<float4> rayBuffer = ResourceDescriptorHeap[PassCB.rayBufferIdx];

	rayBuffer[probeIdx * DDGIVolumeCB.maxRaysPerProbe + rayIdx] = float4(payload.irradiance, payload.distance);
}

[shader("miss")]
void DDGI_Miss(inout RTR_Payload payload)
{
	TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
	payload.irradiance = envMap.SampleLevel(LinearWrapSampler, WorldRayDirection(), 0).rgb;
	payload.distance = FLT_MAX;
}

[shader("closesthit")]
void DDGI_ClosestHit(inout RTR_Payload payload, in HitAttributes attribs)
{


}