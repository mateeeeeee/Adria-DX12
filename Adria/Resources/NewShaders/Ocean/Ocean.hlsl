#include "../CommonResources.hlsli"

#define LAMBDA 1.2

struct OceanIndices
{
	uint displacementIdx;
	uint normalIdx;
	uint foamIdx;
};
ConstantBuffer<OceanIndices> PassCB2 : register(b1);

struct OceanConstants
{
	row_major matrix oceanModelMatrix;
	float3 oceanColor;
};
ConstantBuffer<OceanConstants> PassCB : register(b2);


struct VS_INPUT
{
	float3 Pos : POSITION;
	float2 Uvs : TEX;
};

struct VS_OUTPUT
{
	float4 Position     : SV_POSITION;
	float4 WorldPos     : POS;
	float2 TexCoord     : TEX;
};

VS_OUTPUT OceanVS(VS_INPUT input)
{
	Texture2D displacementTx = ResourceDescriptorHeap[PassCB2.displacementIdx];
	
	float4 worldPos = mul(float4(input.Pos, 1.0), PassCB.oceanModelMatrix);
	worldPos /= worldPos.w;

	float3 displacement = displacementTx.SampleLevel(LinearWrapSampler, input.Uvs, 0.0f).xyz;
	worldPos.xyz += LAMBDA * displacement;

	VS_OUTPUT vout;
	vout.Position = mul(worldPos, FrameCB.viewProjection);
	vout.TexCoord = input.Uvs;
	vout.WorldPos = worldPos;
	return vout;
}

float4 OceanPS(VS_OUTPUT input) : SV_TARGET
{
	Texture2D	normalTx		= ResourceDescriptorHeap[PassCB2.normalIdx];
	TextureCube skyCubeTx		= ResourceDescriptorHeap[FrameCB.envMapIdx];
	Texture2D   foamTx			= ResourceDescriptorHeap[PassCB2.foamIdx];

	float4 normalAndFoam = normalTx.Sample(LinearWrapSampler, input.TexCoord);
	float  foamFactor = normalAndFoam.a;
	float3 n = normalize(normalAndFoam.xyz);

	float3 worldV = FrameCB.cameraPosition.xyz - input.WorldPos.xyz;

	float3 v = normalize(worldV);
	float3 l = reflect(-v, n);

	const float F0 = 0.020018673;
	float F = F0 + (1.0 - F0) * pow(1.0 - dot(n, l), 5.0);

	float3 skyColor = skyCubeTx.Sample(LinearWrapSampler, l).xyz;
	float3 oceanColor = PassCB.oceanColor;

	float3 sky = F * skyColor;
	float dif = clamp(dot(n, normalize(FrameCB.sunDirection.xyz)), 0.f, 1.f);
	float3 water = (1.f - F) * oceanColor * skyColor * dif;

	water += foamFactor * foamTx.Sample(LinearWrapSampler, input.TexCoord).rgb; 
	float3 color = sky + water;
	float spec = pow(clamp(dot(normalize(FrameCB.sunDirection.xyz), l), 0.0, 1.0), 128.0);
	return float4(color + spec * FrameCB.sunColor.xyz, 1.0f);
}


