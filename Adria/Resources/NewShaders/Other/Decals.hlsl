#include "CommonResources.hlsli"

#define DECAL_XY 0
#define DECAL_YZ 1
#define DECAL_XZ 2

struct DecalsConstants
{
	row_major matrix modelMatrix;
	row_major matrix transposedInverseModel;
	uint decalType;
	uint decalAlbedoIdx;
	uint decalNormalIdx;
	uint depthIdx;
};
ConstantBuffer<DecalsConstants> PassCB : register(b2);

struct VSInput
{
	float3 Pos : POSITION;
};

struct VSToPS
{
	float4 Position     : SV_POSITION;
	float4 ClipSpacePos : POSITION;
	matrix InverseModel : INVERSE_MODEL;
};

VSToPS DecalsVS(VSInput input)
{
	VSToPS output = (VSToPS)0;
	float4 worldPos = mul(float4(input.Pos, 1.0f), PassCB.modelMatrix);
	output.Position = mul(worldPos, FrameCB.viewProjection);
	output.ClipSpacePos = output.Position;
	output.InverseModel = transpose(PassCB.transposedInverseModel);
	return output;
}

struct PSOutput
{
	float4 DiffuseRoughness : SV_TARGET0;
#ifdef DECAL_MODIFY_NORMALS
	float4 NormalMetallic   : SV_TARGET1;
#endif
};

PSOutput DecalsPS(VSToPS input)
{
	PSOutput output = (PSOutput)0;

	Texture2D<float4> albedoTx = ResourceDescriptorHeap[PassCB.decalAlbedoIdx];
	Texture2D<float>  depthTx  = ResourceDescriptorHeap[PassCB.depthIdx];

	float2 screenPos = input.ClipSpacePos.xy / input.ClipSpacePos.w;
	float2 depthCoords = screenPos * float2(0.5f, -0.5f) + 0.5f;
	float  depth = depthTx.Sample(PointClampSampler, depthCoords).r;

	float4 posVS = float4(GetViewPosition(depthCoords, depth), 1.0f);
	float4 posWS = mul(posVS, FrameCB.inverseView);
	float4 posLS = mul(posWS, input.InverseModel);
	posLS.xyz /= posLS.w;
	clip(0.5f - abs(posLS.xyz));

	float2 texCoords = 0.0f;
	switch (PassCB.decalType)
	{
	case DECAL_XY:
		texCoords = posLS.xy + 0.5f;
		break;
	case DECAL_YZ:
		texCoords = posLS.yz + 0.5f;
		break;
	case DECAL_XZ:
		texCoords = posLS.xz + 0.5f;
		break;
	default:
		output.DiffuseRoughness.rgb = float3(1, 0, 0);
		return output;
	}

	float4 albedo = albedoTx.SampleLevel(LinearWrapSampler, texCoords, 0);
	if (albedo.a < 0.1) discard;
	output.DiffuseRoughness.rgb = albedo.rgb;

#ifdef DECAL_MODIFY_NORMALS
	Texture2D<float4> normalTx = ResourceDescriptorHeap[PassCB.decalNormalIdx];
	posWS /= posWS.w;
	float3 ddxWorldSpace = ddx(posWS.xyz);
	float3 ddyWorldSpace = ddy(posWS.xyz);

	float3 normal = normalize(cross(ddxWorldSpace, ddyWorldSpace));
	float3 binormal = normalize(ddxWorldSpace);
	float3 tangent = normalize(ddyWorldSpace);

	float3x3 TBN = float3x3(tangent, binormal, normal);

	float3 DecalNormal = normalTx.Sample(LinearWrapSampler, texCoords).xyz;
	DecalNormal = 2.0f * DecalNormal - 1.0f;
	DecalNormal = mul(DecalNormal, TBN);
	float3 DecalNormalVS = normalize(mul(DecalNormal, (float3x3)FrameCB.view));
	output.NormalMetallic.rgb = 0.5 * DecalNormalVS + 0.5;
#endif
	return output;
}