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

struct DDGIPayload
{
	float3 radiance;
	float  distance;
};


[shader("raygeneration")]
void DDGI_RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	StructuredBuffer<DDGIVolume> ddgiVolumes = ResourceDescriptorHeap[FrameCB.ddgiVolumesIdx];
	DDGIVolume ddgiVolume = ddgiVolumes[0];

	uint const probeIdx = DispatchRaysIndex().y;
	uint const rayIdx = DispatchRaysIndex().x;

	float3x3 randomRotation  = AngleAxis3x3(PassCB.randomAngle, PassCB.randomVector);
	float3   randomDirection = normalize(mul(SphericalFibonacci(rayIdx, ddgiVolume.raysPerProbe), randomRotation));

	float3 probeLocation = GetProbeLocation(ddgiVolume, probeIdx);

	RayDesc ray;
	ray.Origin = probeLocation;
	ray.Direction = randomDirection;
	ray.TMin = 0.01f;
	ray.TMax = FLT_MAX;

	DDGIPayload payload;
	payload.radiance = float3(0, 0, 0);
	payload.distance = Max(ddgiVolume.probeSize) * 2;
	TraceRay(tlas, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
	RWBuffer<float4> rayBuffer = ResourceDescriptorHeap[PassCB.rayBufferIdx];
	rayBuffer[probeIdx * ddgiVolume.maxRaysPerProbe + rayIdx] = float4(payload.radiance, payload.distance);
}

[shader("miss")]
void DDGI_Miss(inout DDGIPayload payload)
{
	TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
	payload.radiance = envMap.SampleLevel(LinearWrapSampler, WorldRayDirection(), 0).rgb;
}

[shader("closesthit")]
void DDGI_ClosestHit(inout DDGIPayload payload, in HitAttributes attribs)
{
	StructuredBuffer<DDGIVolume> ddgiVolumes = ResourceDescriptorHeap[FrameCB.ddgiVolumesIdx];
	DDGIVolume ddgiVolume = ddgiVolumes[0];

	Instance instanceData = GetInstanceData(InstanceIndex());
	Mesh meshData = GetMeshData(instanceData.meshIndex);
	Material materialData = GetMaterialData(instanceData.materialIdx);
	VertexData vertex = LoadVertexData(meshData, PrimitiveIndex(), attribs.barycentrics);

	float4 posWS = mul(float4(vertex.pos, 1.0), instanceData.worldMatrix);
	float3 worldPosition = posWS.xyz / posWS.w;
	float3 worldNormal = mul(vertex.nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
	float3 V = normalize(FrameCB.cameraPosition.xyz - worldPosition.xyz);

	MaterialProperties matProperties = GetMaterialProperties(materialData, vertex.uv, 0);
	BrdfData brdfData = GetBrdfData(matProperties);

	float3 N = normalize(worldNormal);

	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	Light light = lights[0];
	bool visibility = TraceShadowRay(light, worldPosition.xyz, FrameCB.inverseView);

	if (!visibility) return;
	float3 L = mul(light.direction.xyz, (float3x3) FrameCB.inverseView);
	L = normalize(-L);
	float3 diffuse = saturate(dot(L, N)) * DiffuseBRDF(brdfData.Diffuse);
	float3 radiance = diffuse * light.color.rgb;

	radiance += matProperties.emissive;
	radiance += DiffuseBRDF(min(brdfData.Diffuse, 0.9f)) * SampleDDGIIrradiance(ddgiVolume, worldPosition, N, WorldRayDirection());
	
	payload.radiance = radiance;
	payload.distance = min(RayTCurrent(), payload.distance);
}