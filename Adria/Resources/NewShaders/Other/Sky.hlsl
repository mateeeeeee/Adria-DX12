#include "../CommonResources.hlsli"



struct SkyConstants
{
	row_major matrix modelMatrix;
};
ConstantBuffer<SkyConstants> SkyPassCB : register(b2);

struct VertexIn
{
	float3 PosL : POSITION;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosL : POSITION;
};

VertexOut SkyVS(VertexIn input)
{
	VertexOut output;
	output.PosH = mul(float4(input.PosL, 1.0f), SkyPassCB.modelMatrix);
	output.PosH = mul(output.PosH, FrameCB.viewProjection).xyww;
	output.PosL = input.PosL;
	return output;
}


struct SkyboxConstants
{
	uint cubemapIdx;
};
ConstantBuffer<SkyboxConstants> SkyboxPassCB : register(b1);

float4 SkyboxPS(VertexOut input) : SV_Target
{
	TextureCube cubemapTx = ResourceDescriptorHeap[SkyboxPassCB.cubemapIdx];
	return cubemapTx.Sample(LinearWrapSampler, input.PosL);
}


struct HosekWilkieConstants
{
	float3 A;
	float3 B;
	float3 C;
	float3 D;
	float3 E;
	float3 F;
	float3 G;
	float3 H;
	float3 I;
	float3 Z;
};
ConstantBuffer<HosekWilkieConstants> HosekWilkiePassCB : register(b3);

float3 HosekWilkie(float cosTheta, float gamma, float cosGamma)
{
	const float3 A = HosekWilkiePassCB.A;
	const float3 B = HosekWilkiePassCB.B;
	const float3 C = HosekWilkiePassCB.C;
	const float3 D = HosekWilkiePassCB.D;
	const float3 E = HosekWilkiePassCB.E;
	const float3 F = HosekWilkiePassCB.F;
	const float3 G = HosekWilkiePassCB.G;
	const float3 H = HosekWilkiePassCB.H;
	const float3 I = HosekWilkiePassCB.I;

	float3 chi = (1 + cosGamma * cosGamma) / pow(1 + H * H - 2 * cosGamma * H, float3(1.5f, 1.5f, 1.5f));
	return (1 + A * exp(B / (cosTheta + 0.01))) * (C + D * exp(E * gamma) + F * (cosGamma * cosGamma) + G * chi + I * sqrt(cosTheta));
}
float3 HosekWilkieSky(float3 v, float3 sunDir)
{
	float cosTheta = clamp(v.y, 0, 1);
	float cosGamma = clamp(dot(v, sunDir), 0, 1);
	float gamma = acos(cosGamma);
	float3 R = -HosekWilkiePassCB.Z * HosekWilkie(cosTheta, gamma, cosGamma);
	return R;
}

float4 HosekWilkieSkyPS(VertexOut input) : SV_TARGET
{
	float3 dir = normalize(input.PosL);
	float3 col = HosekWilkieSky(dir, normalize(FrameCB.sunDirection.xyz));
	return float4(col, 1.0);
}

struct UniformColorSkyConstants
{
	float3 skyColor;
};
ConstantBuffer<UniformColorSkyConstants> UniformColorSkyPassCB : register(b1);

float4 UniformColorSkyPS(VertexOut) : SV_Target
{
	return float4(UniformColorSkyPassCB.skyColor, 1.0f);
}