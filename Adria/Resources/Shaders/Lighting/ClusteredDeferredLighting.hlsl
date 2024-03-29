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
	uint lightIndexListIdx;
	uint lightGridIdx;
	uint normalMetallicIdx;
	uint diffuseIdx;
	uint depthIdx;
	uint emissiveIdx;
	uint aoIdx;
	uint outputIdx;
};
ConstantBuffer<ClusteredDeferredLightingConstants> PassCB : register(b1);

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
	Texture2D               normalMetallicTx = ResourceDescriptorHeap[PassCB.normalMetallicIdx];
	Texture2D               diffuseTx		 = ResourceDescriptorHeap[PassCB.diffuseIdx];
	Texture2D<float>        depthTx			 = ResourceDescriptorHeap[PassCB.depthIdx];
	StructuredBuffer<Light> lights			 = ResourceDescriptorHeap[FrameCB.lightsIdx];
	StructuredBuffer<uint>  lightIndexList	 = ResourceDescriptorHeap[PassCB.lightIndexListIdx];
	StructuredBuffer<LightGrid> lightGrid	 = ResourceDescriptorHeap[PassCB.lightGridIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	float4 normalMetallic = normalMetallicTx.Sample(LinearWrapSampler, uv);
	float3 viewNormal = 2.0f * normalMetallic.rgb - 1.0f;
	float  metallic = normalMetallic.a;
	float  depth = depthTx.Sample(LinearWrapSampler, uv);

	float3 viewPosition = GetViewPosition(uv, depth);
	float4 albedoRoughness = diffuseTx.Sample(LinearWrapSampler, uv);
	float3 V = normalize(float3(0.0f, 0.0f, 0.0f) - viewPosition);
	float3 albedo = albedoRoughness.rgb;
	float  roughness = albedoRoughness.a;
	float linearDepth = LinearizeDepth(depth);


	uint zCluster = uint(max((log2(linearDepth) - log2(FrameCB.cameraNear)) * 16.0f / log2(FrameCB.cameraFar / FrameCB.cameraNear), 0.0f));
	uint2 clusterDim = ceil(FrameCB.renderResolution / float2(16, 16));
	uint3 tiles = uint3(uint2(((float2) input.DispatchThreadId.xy + 0.5f) / clusterDim), zCluster);

	uint tileIndex = tiles.x +
		16 * tiles.y +
		(256) * tiles.z;

	uint lightCount = lightGrid[tileIndex].lightCount;
	uint lightOffset = lightGrid[tileIndex].offset;
	
	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);
	LightingResult lightResult = (LightingResult)0;
	for (uint i = 0; i < lightCount; i++)
	{
		uint lightIndex = lightIndexList[lightOffset + i];
		Light light = lights[lightIndex];
		if (!light.active) continue;
        lightResult = lightResult + DoLight(light, brdfData, viewPosition, viewNormal, V, uv);
    }

	Texture2D<float> aoTx = ResourceDescriptorHeap[PassCB.aoIdx];
	float ambientOcclusion = aoTx.Sample(LinearWrapSampler, uv);
	float3 indirectLighting = GetIndirectLighting(viewPosition, viewNormal, brdfData.Diffuse, ambientOcclusion);

	Texture2D emissiveTx = ResourceDescriptorHeap[PassCB.emissiveIdx];
	float4 emissiveData = emissiveTx.Sample(LinearWrapSampler, uv);
	float3 emissiveColor = emissiveData.rgb * emissiveData.a * 256;

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[input.DispatchThreadId.xy] = float4(indirectLighting + lightResult.Diffuse + lightResult.Specular + emissiveColor, 1.0f);
}