#include "Lighting.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16


struct RendererOutputConstants
{
	uint normalMetallicIdx;
	uint diffuseIdx;
	uint depthIdx;
	uint emissiveIdx;
	uint aoIdx;
	uint outputIdx;
};
ConstantBuffer<RendererOutputConstants> RendererOutputPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void RendererOutputCS(CSInput input)
{
	Texture2D               normalMetallicTexture = ResourceDescriptorHeap[RendererOutputPassCB.normalMetallicIdx];
	Texture2D               diffuseTexture		  = ResourceDescriptorHeap[RendererOutputPassCB.diffuseIdx];
	Texture2D<float>        depthTexture		  = ResourceDescriptorHeap[RendererOutputPassCB.depthIdx];
	StructuredBuffer<Light> lightBuffer		      = ResourceDescriptorHeap[FrameCB.lightsIdx];
	RWTexture2D<float4> outputTexture			  = ResourceDescriptorHeap[RendererOutputPassCB.outputIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
	
#if OUTPUT_DIFFUSE
	float4 albedoRoughness	= diffuseTexture.Sample(LinearWrapSampler, uv);
	float3 albedo = albedoRoughness.rgb;
	outputTexture[input.DispatchThreadId.xy] = float4(albedo, 1.0f);

#elif OUTPUT_NORMALS
	float4 normalMetallic = normalMetallicTexture.Sample(LinearWrapSampler, uv);
	float3 viewNormal	  = 2.0f * normalMetallic.rgb - 1.0f;
	float3 worldNormal = mul(viewNormal, (float3x3)transpose(FrameCB.view));
	outputTexture[input.DispatchThreadId.xy] = float4(worldNormal, 1.0f); 

#elif OUTPUT_ROUGHNESS
	float4 albedoRoughness	= diffuseTexture.Sample(LinearWrapSampler, uv);
	float roughness = albedoRoughness.a;
	outputTexture[input.DispatchThreadId.xy] = float4(roughness, roughness, roughness, 1.0f);

#elif OUTPUT_METALLIC
	float4 normalMetallic = normalMetallicTexture.Sample(LinearWrapSampler, uv);
	float metallic = normalMetallic.a;
	outputTexture[input.DispatchThreadId.xy] = float4(metallic, metallic, metallic, 1.0f);

#elif OUTPUT_EMISSIVE
	Texture2D emissiveTexture = ResourceDescriptorHeap[RendererOutputPassCB.emissiveIdx];
	float4 emissiveData = emissiveTexture.Sample(LinearWrapSampler, uv);
	float3 emissiveColor = emissiveData.rgb * emissiveData.a * 256;
	outputTexture[input.DispatchThreadId.xy] = float4(emissiveColor, 1.0f);

#elif OUTPUT_AO
	Texture2D<float> ambientOcclusionTexture = ResourceDescriptorHeap[RendererOutputPassCB.aoIdx];
	float ambientOcclusion = ambientOcclusionTexture.Sample(LinearWrapSampler, uv);
	outputTexture[input.DispatchThreadId.xy] = float4(ambientOcclusion, ambientOcclusion, ambientOcclusion, 1.0f);

#elif OUTPUT_INDIRECT
	float4 albedoRoughness	= diffuseTexture.Sample(LinearWrapSampler, uv);
	float3 albedo		    = albedoRoughness.rgb;
	float roughness		    = albedoRoughness.a;
	float4 normalMetallic   = normalMetallicTexture.Sample(LinearWrapSampler, uv);
	float3 viewNormal	    = 2.0f * normalMetallic.rgb - 1.0f;
	float metallic		    = normalMetallic.a;
	BrdfData brdfData	    = GetBrdfData(albedo, metallic, roughness);
	float  depth		    = depthTexture.Sample(LinearWrapSampler, uv);
	float3 viewPosition		= GetViewPosition(uv, depth);

	Texture2D<float> ambientOcclusionTexture = ResourceDescriptorHeap[RendererOutputPassCB.aoIdx];
	float ambientOcclusion = ambientOcclusionTexture.Sample(LinearWrapSampler, uv);
	outputTexture[input.DispatchThreadId.xy] = float4(ambientOcclusion, ambientOcclusion, ambientOcclusion, 1.0f);

	float3 indirectLighting = GetIndirectLighting(viewPosition, viewNormal, brdfData.Diffuse, ambientOcclusion);

	outputTexture[input.DispatchThreadId.xy] = float4(indirectLighting, 1.0f); 
#else 
	outputTexture[input.DispatchThreadId.xy] = float4(1.0f, 0.0f, 0.0f, 1.0f); 
#endif
}