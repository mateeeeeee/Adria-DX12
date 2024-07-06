#include "CommonResources.hlsli"


struct Constants
{
	row_major matrix modelMatrix;
	float3 diffuseColor;
	uint   diffuseIdx;
};
ConstantBuffer<Constants> PassCB : register(b2);

struct VSInput
{
	float3 Pos : POSITION;
	float2 Uv  : TEX;
};

struct VSToPS
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEX;
};

VSToPS SimpleVS(VSInput input)
{
	VSToPS output;
	output.Position = mul(mul(float4(input.Pos, 1.0), PassCB.modelMatrix), FrameCB.viewProjection);
	output.TexCoord = input.Uv;
	return output;
}

VSToPS SunVS(VSInput input)
{
	float4x4 modelMatrix = PassCB.modelMatrix;
	modelMatrix[3][0] += FrameCB.inverseView[3][0];
	modelMatrix[3][1] += FrameCB.inverseView[3][1];
	modelMatrix[3][2] += FrameCB.inverseView[3][2];

	float4x4 modelView = mul(modelMatrix, FrameCB.view);
	modelView[0][0] = 1;
	modelView[0][1] = 0;
	modelView[0][2] = 0;
	modelView[1][0] = 0;
	modelView[1][1] = 1;
	modelView[1][2] = 0;
	modelView[2][0] = 0;
	modelView[2][1] = 0;
	modelView[2][2] = 1;

	float4 viewPosition = mul(float4(input.Pos, 1.0), modelView);
	float4 PosH = mul(viewPosition, FrameCB.projection);

	VSToPS output;
	output.Position = float4(PosH.xy, 0.001f, PosH.w); 
	output.TexCoord = input.Uv;
	return output;
}

float4 TexturePS(VSToPS input) : SV_TARGET
{
	Texture2D diffuseTx = ResourceDescriptorHeap[PassCB.diffuseIdx];
	float4 texColor = diffuseTx.Sample(LinearWrapSampler, input.TexCoord) * float4(PassCB.diffuseColor, 1.0);
	return texColor;
}

float4 SolidPS(VSToPS input) : SV_TARGET
{
	return float4(PassCB.diffuseColor, 1.0);
}