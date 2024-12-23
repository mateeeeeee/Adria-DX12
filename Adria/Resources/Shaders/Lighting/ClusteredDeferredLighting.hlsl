#include "Lighting.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16

struct LightGrid
{
	uint offset;
	uint lightCount;
};

struct ClusteredDeferredLightingConstants
{
	uint normalIdx;
	uint diffuseIdx;
	uint emissiveIdx;
	uint customIdx;
	uint depthIdx;
	uint aoIdx;
	uint outputIdx;
	uint lightBufferDataPacked;
};
ConstantBuffer<ClusteredDeferredLightingConstants> ClusteredDeferredLightingPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ClusteredDeferredLightingCS(CSInput input)
{
	Texture2D               normalRT		 = ResourceDescriptorHeap[ClusteredDeferredLightingPassCB.normalIdx];
	Texture2D               diffuseRT		 = ResourceDescriptorHeap[ClusteredDeferredLightingPassCB.diffuseIdx];
	Texture2D				emissiveRT		 = ResourceDescriptorHeap[ClusteredDeferredLightingPassCB.emissiveIdx];
	Texture2D				customRT		 = ResourceDescriptorHeap[ClusteredDeferredLightingPassCB.customIdx];
	Texture2D<float>        depthTexture	 = ResourceDescriptorHeap[ClusteredDeferredLightingPassCB.depthIdx];

	uint2 lightBufferData = UnpackTwoUint16FromUint32(ClusteredDeferredLightingPassCB.lightBufferDataPacked);
	
	StructuredBuffer<uint>  lightIndexList		= ResourceDescriptorHeap[lightBufferData.x];
	StructuredBuffer<LightGrid> lightGridBuffer	= ResourceDescriptorHeap[lightBufferData.y];
	StructuredBuffer<Light> lightBuffer			= ResourceDescriptorHeap[FrameCB.lightsIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	float3 viewNormal;
	float metallic;
	uint  shadingExtension;
	float4 normalRTData = normalRT.Sample(LinearWrapSampler, uv);
	DecodeGBufferNormalRT(normalRTData, viewNormal, metallic, shadingExtension);
	float  depth = depthTexture.Sample(LinearWrapSampler, uv);

	float3 viewPosition = GetViewPosition(uv, depth);
	float3 V = normalize(float3(0.0f, 0.0f, 0.0f) - viewPosition);
	float4 albedoRoughness = diffuseRT.Sample(LinearWrapSampler, uv);
	float3 albedo = albedoRoughness.rgb;
	float  roughness = albedoRoughness.a;
	float4 customData = customRT.Sample(LinearWrapSampler, uv);

	float  linearDepth = LinearizeDepth(depth);
	
	float nearPlane = min(FrameCB.cameraNear, FrameCB.cameraFar);
	float farPlane = max(FrameCB.cameraNear, FrameCB.cameraFar);

	uint zCluster = uint(max((log2(linearDepth) - log2(nearPlane)) * 16.0f / log2(farPlane / nearPlane), 0.0f));
	uint2 clusterDim = ceil(FrameCB.renderResolution / float2(16, 16));
	uint3 tiles = uint3(uint2(((float2) input.DispatchThreadId.xy + 0.5f) / clusterDim), zCluster);

	uint tileIndex = tiles.x + 16 * tiles.y + (256) * tiles.z;

	uint lightCount = lightGridBuffer[tileIndex].lightCount;
	uint lightOffset = lightGridBuffer[tileIndex].offset;
	
	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);
	float3 directLighting = 0.0f;
	for (uint i = 0; i < lightCount; i++)
	{
		uint lightIndex = lightIndexList[lightOffset + i];
		Light light = lightBuffer[lightIndex];
		if (!light.active) continue;
        directLighting += DoLight(shadingExtension, light, brdfData, viewPosition, viewNormal, V, uv, customData);
    }

	Texture2D<float> ambientOcclusionTexture = ResourceDescriptorHeap[ClusteredDeferredLightingPassCB.aoIdx];
	float ambientOcclusion = ambientOcclusionTexture.Sample(LinearWrapSampler, uv);
	float3 indirectLighting = GetIndirectLighting(viewPosition, viewNormal, brdfData.Diffuse, ambientOcclusion);

	float4 emissiveData = emissiveRT.Sample(LinearWrapSampler, uv);
	float3 emissiveColor = emissiveData.rgb * emissiveData.a * 256;

	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[ClusteredDeferredLightingPassCB.outputIdx];
	outputTexture[input.DispatchThreadId.xy] = float4(indirectLighting + directLighting + emissiveColor, 1.0f);
}