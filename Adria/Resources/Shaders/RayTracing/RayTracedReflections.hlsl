#include "Constants.hlsli"
#include "Scene.hlsli"
#include "Lighting.hlsli"
#include "Reflections.hlsli"
#include "RayTracingUtil.hlsli"

struct RayTracedReflectionsConstants
{
    float roughnessScale;
    uint  depthIdx;
	uint  normalIdx;
	uint  albedoIdx;
	uint  outputIdx;
};
ConstantBuffer<RayTracedReflectionsConstants> RayTracedReflectionsPassCB : register(b1);

struct [raypayload] RTR_Payload
{
	float3 reflectionColor: write(caller, closesthit, miss) : read(caller);
};

[shader("raygeneration")]
void RTR_RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[RayTracedReflectionsPassCB.outputIdx];
	Texture2D<float> depthTexture = ResourceDescriptorHeap[RayTracedReflectionsPassCB.depthIdx];
	Texture2D normalMetallicTexture = ResourceDescriptorHeap[RayTracedReflectionsPassCB.normalIdx];
	Texture2D albedoRoughnessTexture = ResourceDescriptorHeap[RayTracedReflectionsPassCB.albedoIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float2 uv = (launchIndex + 0.5f) / FrameCB.renderResolution;
	float depth = depthTexture.Load(int3(launchIndex.xy, 0));
	float4 normalMetallic = normalMetallicTexture.Load(int3(launchIndex.xy, 0));
	float roughness = albedoRoughnessTexture.Load(int3(launchIndex.xy, 0)).a;
	float reflectivity = saturate((1 - roughness) * (1 - roughness));
	if (depth > 0.00001f && reflectivity > 0.0f)
	{
		float3 viewNormal = normalMetallic.xyz;
		viewNormal = 2.0f * viewNormal - 1.0f;
		float3 worldNormal = normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view)));
		float3 worldPosition = GetWorldPosition(uv, depth);

		float3 V = normalize(worldPosition - FrameCB.cameraPosition.xyz);
		float3 rayDir = reflect(V, worldNormal);

		RNG rng = RNG_Initialize(launchIndex.x + launchIndex.y * launchDim.x, 0, 16);
		rayDir = GetConeSample(rng, rayDir, RayTracedReflectionsPassCB.roughnessScale);

		RayDesc ray;
		ray.Origin = worldPosition;
		ray.Direction = rayDir;
		ray.TMin = 1e-2f;
		ray.TMax = FLT_MAX;

		RTR_Payload payloadData;
		payloadData.reflectionColor = 0.0f;
		TraceRay(tlas, RAY_FLAG_FORCE_OPAQUE, 0xFF, 0, 0, 0, ray, payloadData);
		outputTexture[launchIndex.xy] = float4(reflectivity * payloadData.reflectionColor, 1.0f);
	}
	else
	{
		outputTexture[launchIndex.xy] = 0.0f;
	}
}

[shader("miss")]
void RTR_Miss(inout RTR_Payload payloadData)
{
    TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
    payloadData.reflectionColor = envMap.SampleLevel(LinearWrapSampler, WorldRayDirection(), 0).rgb;
}

[shader("closesthit")]
void RTR_ClosestHitPrimaryRay(inout RTR_Payload payloadData, in HitAttributes attribs)
{
	Instance instanceData = GetInstanceData(InstanceIndex());
	Mesh meshData = GetMeshData(instanceData.meshIndex);
	Material materialData = GetMaterialData(instanceData.materialIdx);

	VertexData vertex = LoadVertexData(meshData, PrimitiveIndex(), attribs.barycentrics);

	float4 posWS = mul(float4(vertex.pos, 1.0), instanceData.worldMatrix);
	float3 worldPosition = posWS.xyz / posWS.w;
    float3 worldNormal = mul(vertex.nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
	float3 V = normalize(FrameCB.cameraPosition.xyz - worldPosition.xyz);

	MaterialProperties materialProperties = GetMaterialProperties(materialData, vertex.uv, 2);
	float3 albedoColor = materialProperties.baseColor;
	float roughness = materialProperties.roughness;
	float metallic = materialProperties.metallic;

	float3 radiance = 0.0f;
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	for (int i = 0; i < FrameCB.lightCount; ++i)
	{
		Light light = lights[i];
		bool visibility = TraceShadowRay(light, worldPosition.xyz, FrameCB.inverseView);
		if(!visibility) continue;

		
		float3 directLighting = DoLightNoShadows_Default(light, worldPosition.xyz, normalize(worldNormal), V, albedoColor.xyz, metallic, roughness);
		radiance += directLighting;
	}
	radiance += materialProperties.emissive;
	radiance += GetIndirectLightingWS(worldPosition, worldNormal, albedoColor, 1.0f);
	payloadData.reflectionColor = radiance;
}


