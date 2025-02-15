#include "Lighting.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16

struct DeferredLightingConstants
{
	uint normalIdx;
	uint diffuseIdx;
	uint emissiveIdx;
	uint customIdx;
	uint depthIdx;
	uint aoIdx;
	uint outputIdx;
};
ConstantBuffer<DeferredLightingConstants> DeferredLightingPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void DeferredLightingCS(CSInput input)
{
	Texture2D               normalRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.normalIdx];
	Texture2D               diffuseRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.diffuseIdx];
	Texture2D				emissiveRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.emissiveIdx];
	Texture2D               customRT			  = ResourceDescriptorHeap[DeferredLightingPassCB.customIdx];
	Texture2D<float>        depthTexture		  = ResourceDescriptorHeap[DeferredLightingPassCB.depthIdx];
	Texture2D<float>		ambientOcclusionTexture = ResourceDescriptorHeap[DeferredLightingPassCB.aoIdx];
	
	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	float3 viewNormal;
	float metallic;
	uint  shadingExtension;
	float4 normalRTData = normalRT.Sample(LinearWrapSampler, uv);
	DecodeGBufferNormalRT(normalRTData, viewNormal, metallic, shadingExtension);
	float  depth		  = depthTexture.Sample(LinearWrapSampler, uv);

	float3 viewPosition		= GetViewPosition(uv, depth);
	float3 V				= normalize(float3(0.0f, 0.0f, 0.0f) - viewPosition);

	float4 albedoRoughness	= diffuseRT.Sample(LinearWrapSampler, uv);
	float3 albedo			= albedoRoughness.rgb;
	float  roughness		= albedoRoughness.a;
	float4 customData       = customRT.Sample(LinearWrapSampler, uv);
	
	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);
	float3 directLighting = 0.0f;
	for (uint i = 0; i < FrameCB.lightCount; ++i)
	{
		LightInfo lightInfo = LoadLightInfo(i); 
		if (!lightInfo.active) continue;
        directLighting += DoLight(shadingExtension, lightInfo, brdfData, viewPosition, viewNormal, V, uv, customData);
    }

	float ambientOcclusion = ambientOcclusionTexture.Sample(LinearWrapSampler, uv);
	float3 indirectLighting = GetIndirectLighting(viewPosition, viewNormal, brdfData.Diffuse, ambientOcclusion);
	
	float4 emissiveData = emissiveRT.Sample(LinearWrapSampler, uv);
	float3 emissiveColor = emissiveData.rgb * emissiveData.a * 256;

	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[DeferredLightingPassCB.outputIdx];
	outputTexture[input.DispatchThreadId.xy] = float4(indirectLighting + directLighting + emissiveColor, 1.0f);
}