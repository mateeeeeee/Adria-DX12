#include "CommonResources.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16

struct AmbientConstants
{
	uint  color;
	uint  normalMetallicIdx;
	uint  diffuseIdx;
	uint  emissiveIdx;
	uint  depthIdx;
	int   aoIdx;
	uint  outputIdx;
};
ConstantBuffer<AmbientConstants> AmbientPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void AmbientCS(CSInput input)
{
	Texture2D               normalMetallicTexture = ResourceDescriptorHeap[AmbientPassCB.normalMetallicIdx];
	Texture2D               diffuseTexture        = ResourceDescriptorHeap[AmbientPassCB.diffuseIdx];
	Texture2D               emissiveTexture       = ResourceDescriptorHeap[AmbientPassCB.emissiveIdx];
	Texture2D<float>        depthTexture          = ResourceDescriptorHeap[AmbientPassCB.depthIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	if (all(uv) >= 0.0f && all(uv) <= 1.0f)
	{
		float4 albedoRoughness = diffuseTexture.Sample(LinearWrapSampler, uv);
		float3 albedo = albedoRoughness.rgb;
		float4 emissiveData = emissiveTexture.Sample(LinearWrapSampler, uv);
		float3 emissive = emissiveData.rgb;
		float emissiveFactor = emissiveData.a * 256;
		emissive = emissive * emissiveFactor;
		float ao = 1.0f;
		if (AmbientPassCB.aoIdx >= 0)
		{
			Texture2D<float> aoTx = ResourceDescriptorHeap[AmbientPassCB.aoIdx];
			ao = aoTx.Sample(LinearWrapSampler, uv);
		}
		float4 ambientColor = UnpackUintColor(AmbientPassCB.color);
		RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[AmbientPassCB.outputIdx];
		outputTexture[input.DispatchThreadId.xy] = ambientColor * float4(albedo, 1.0f) * ao + float4(emissive.rgb, 1.0f);
	}
}