#include "../CommonResources.hlsli"
#include "../Lighting.hlsli"

struct ShadowConstants
{
	uint  lightIndex;
	uint  matrixIndex;
};
ConstantBuffer<ShadowConstants> PassCB : register(b1);

struct ModelConstants
{
	row_major matrix modelMatrix;
	uint albedoIdx;
};

ConstantBuffer<ModelConstants> ModelCB : register(b2);

struct VS_INPUT
{
	float3 Pos : POSITION;
#if ALPHA_TEST
	float2 TexCoords : TEX;
#endif
};
struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
#if ALPHA_TEST
	float2 TexCoords : TEX;
#endif
};

VS_OUTPUT ShadowVS(VS_INPUT input)
{
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	StructuredBuffer<float4x4> lightViewProjections = ResourceDescriptorHeap[FrameCB.lightsMatricesIdx];
	Light light = lights[PassCB.lightIndex];
	float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + PassCB.matrixIndex];

	VS_OUTPUT output;
	float4 pos = float4(input.Pos, 1.0f);
	pos = mul(pos, ModelCB.modelMatrix);
	pos = mul(pos, lightViewProjection); //transpose it?
	output.Pos = pos;

#if ALPHA_TEST
	output.TexCoords = input.TexCoords;
#endif
	return output;
}

void ShadowPS(VS_OUTPUT input)
{
#if TRANSPARENT 
	Texture2D albedoTx = ResourceDescriptorHeap[ModelCB.albedoIdx];
	if (albedoTx.Sample(LinearWrapSampler, input.TexCoords).a < 0.1) discard;
#endif
}
