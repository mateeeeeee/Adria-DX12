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
	float3 radiance;
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
	payload.radiance = float3(0, 0, 0);
	payload.distance = FLT_MAX;
	TraceRay(tlas, (RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES), 0xFF, 0, 0, 0, ray, payload);
	RWBuffer<float4> rayBuffer = ResourceDescriptorHeap[PassCB.rayBufferIdx];

	rayBuffer[probeIdx * DDGIVolumeCB.maxRaysPerProbe + rayIdx] = float4(payload.radiance, payload.distance);
}

[shader("miss")]
void DDGI_Miss(inout DDGIPayload payload)
{
	TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
	payload.radiance = envMap.SampleLevel(LinearWrapSampler, WorldRayDirection(), 0).rgb;
	payload.distance = FLT_MAX;
}

[shader("closesthit")]
void DDGI_ClosestHit(inout DDGIPayload payload, in HitAttributes attribs)
{
	Instance instanceData = GetInstanceData(InstanceIndex());
	Mesh meshData = GetMeshData(instanceData.meshIndex);
	Material materialData = GetMaterialData(instanceData.materialIdx);

	VertexData vertex = LoadVertexData(meshData, PrimitiveIndex(), attribs.barycentrics);

	float4 posWS = mul(float4(vertex.pos, 1.0), instanceData.worldMatrix);
	float3 worldPosition = posWS.xyz / posWS.w;
	float3 worldNormal = mul(vertex.nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
	float3 V = normalize(FrameCB.cameraPosition.xyz - worldPosition.xyz);

	MaterialProperties matProperties = GetMaterialProperties(materialData, uv, 0);
	BrdfData brdfData = GetBrdfData(matProperties);

	float3 V = normalize(FrameCB.cameraPosition.xyz - worldPosition.xyz);
	float3 N = worldNormal;

	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	Light light = lights[0];
	bool visibility = TraceShadowRay(light, worldPosition.xyz, FrameCB.inverseView);

	if (!visibility) return;

	float3 L = normalize(-light.direction.xyz);
	float3 diffuse = saturate(dot(L, N)) * Diffuse_Lambert(brdfData.Diffuse);
	float3 radiance = diffuse * light.color.rgb;

	radiance += matProperties.emissive;
	radiance += Diffuse_Lambert(min(brdfData.Diffuse, 0.9f)) * SampleDDGIIrradiance(DDGIVolumeCB, worldPosition, N, WorldRayDirection());
	
	payload.radiance = radiance;
	payload.distance = RayTCurrent();
}