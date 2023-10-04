#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

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
ConstantBuffer<AmbientConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void Ambient(CSInput input)
{
	Texture2D               normalMetallicTx = ResourceDescriptorHeap[PassCB.normalMetallicIdx];
	Texture2D               diffuseTx        = ResourceDescriptorHeap[PassCB.diffuseIdx];
	Texture2D               emissiveTx       = ResourceDescriptorHeap[PassCB.emissiveIdx];
	Texture2D<float>        depthTx          = ResourceDescriptorHeap[PassCB.depthIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);

	if (all(uv) >= 0.0f && all(uv) <= 1.0f)
	{
		float4 albedoRoughness = diffuseTx.Sample(LinearWrapSampler, uv);
		float3 albedo = albedoRoughness.rgb;
		float4 emissiveData = emissiveTx.Sample(LinearWrapSampler, uv);
		float3 emissive = emissiveData.rgb;
		float emissiveFactor = emissiveData.a * 256;
		emissive = emissive * emissiveFactor;
		float ao = 1.0f;
		if (PassCB.aoIdx >= 0)
		{
			Texture2D<float> aoTx = ResourceDescriptorHeap[PassCB.aoIdx];
			ao = aoTx.Sample(LinearWrapSampler, uv);
		}
		float4 ambientColor = UnpackUintColor(PassCB.color);
		RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
		outputTx[input.DispatchThreadId.xy] = ambientColor * float4(albedo, 1.0f) * ao + float4(emissive.rgb, 1.0f);
	}
}