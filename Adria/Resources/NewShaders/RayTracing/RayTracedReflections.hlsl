#include "../Constants.hlsli"
#include "../Scene.hlsli"
#include "../Lighting.hlsli"
#include "../Reflections.hlsli"
#include "RayTracingUtil.hlsli"


struct RayTracedReflectionsConstants
{
    float roughnessScale;
    uint  depthIdx;
	uint  normalIdx;
	uint  albedoIdx;
	uint  outputIdx;
};
ConstantBuffer<RayTracedReflectionsConstants> PassCB : register(b1);

struct RTR_Payload
{
	float3 reflectionColor;
	uint   randSeed;
};

[shader("raygeneration")]
void RTR_RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	Texture2D normalMetallicTx = ResourceDescriptorHeap[PassCB.normalIdx];
	Texture2D albedoRoughnessTx = ResourceDescriptorHeap[PassCB.albedoIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float2 uv = (launchIndex + 0.5f) / FrameCB.renderResolution;
	float depth = depthTx.Load(int3(launchIndex.xy, 0));
	float4 normalMetallic = normalMetallicTx.Load(int3(launchIndex.xy, 0));
	float roughness = albedoRoughnessTx.Load(int3(launchIndex.xy, 0)).a;

	if (roughness >= ROUGHNESS_CUTOFF)
	{
		outputTx[launchIndex.xy] = 0.0f;
		return;
	}

	float3 viewNormal = normalMetallic.xyz;
	viewNormal = 2 * viewNormal - 1.0;
	float3 worldNormal = normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view)));
	float3 worldPosition = GetWorldPosition(uv, depth);

	float3 V = normalize(worldPosition - FrameCB.cameraPosition.xyz);
	float3 rayDir = reflect(V, worldNormal);

	uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, 0, 16);
	rayDir = GetConeSample(randSeed, rayDir, PassCB.roughnessScale);

	RayDesc ray;
	ray.Origin = worldPosition;
	ray.Direction = rayDir;
	ray.TMin = 0.02f;
	ray.TMax = FLT_MAX;

	RTR_Payload payloadData;
	payloadData.reflectionColor = 0.0f;
    payloadData.randSeed = randSeed;
	TraceRay(tlas,
		RAY_FLAG_FORCE_OPAQUE,
		0xFF, 0, 0, 0, ray, payloadData);

	float3 fresnel = clamp(pow(1 - dot(normalize(worldPosition), worldNormal), 1), 0, 1);
	outputTx[launchIndex.xy] = float4(0.25f * fresnel * payloadData.reflectionColor, 1.0f);
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

	MaterialProperties materialProperties = GetMaterialProperties(materialData, vertex.uv, 0);
	float3 albedoColor = materialProperties.baseColor;
	float roughness = materialProperties.roughness;
	float metallic = materialProperties.metallic;

	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	Light light = lights[0];
	bool visibility = TraceShadowRay(light, worldPosition.xyz, FrameCB.inverseView);
	
	LightingResult radiance = DoLightNoShadows(light, worldPosition.xyz, normalize(worldNormal), V, albedoColor.xyz, metallic, roughness);
	float3 reflectionColor = (visibility + 0.05f) * (1.0f - roughness) * (radiance.Diffuse + radiance.Specular) + materialProperties.emissive;
	payloadData.reflectionColor = reflectionColor;
}


