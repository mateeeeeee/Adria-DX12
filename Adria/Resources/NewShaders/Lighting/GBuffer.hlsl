#include "../Scene.hlsli"

struct GBufferConstants
{
    uint instanceId;
};
ConstantBuffer<GBufferConstants> PassCB : register(b1);

struct VS_OUTPUT
{
	float4 Position     : SV_POSITION;

	float2 Uvs          : TEX;
	float3 NormalVS     : NORMAL0;
	float3 TangentWS    : TANGENT;
	float3 BitangentWS  : BITANGENT;
	float3 NormalWS     : NORMAL1;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 Uvs : TEX;
	float3 NormalVS : NORMAL0;
	float3 TangentWS : TANGENT;
	float3 BitangentWS : BITANGENT;
	float3 NormalWS : NORMAL1;

	bool IsFrontFace : SV_IsFrontFace;
};

struct PS_OUTPUT
{
	float4 NormalMetallic : SV_TARGET0;
	float4 DiffuseRoughness : SV_TARGET1;
	float4 Emissive : SV_TARGET2;
};

VS_OUTPUT GBufferVS(uint vertexId : SV_VertexID)
{
	VS_OUTPUT Output = (VS_OUTPUT)0;

    Instance instanceData = GetInstanceData(PassCB.instanceId);
    Mesh meshData = GetMeshData(instanceData.meshIndex);

	float3 pos = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, vertexId);
	float2 uv  = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, vertexId);
	float3 nor = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, vertexId);
	float4 tan = LoadMeshBuffer<float4>(meshData.bufferIdx, meshData.tangentsOffset, vertexId);
    
	float4 posWS = mul(float4(pos, 1.0), instanceData.worldMatrix);
	Output.Position = mul(posWS, FrameCB.viewProjection);
	Output.Position.xy += FrameCB.cameraJitter * Output.Position.w;

	Output.Uvs = uv;

	float3 normalWS = mul(nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
	Output.NormalWS = normalWS;
	Output.NormalVS = mul(normalWS, (float3x3) transpose(FrameCB.inverseView));
	Output.TangentWS = mul(tan.xyz, (float3x3) transpose(instanceData.inverseWorldMatrix));
	Output.BitangentWS = normalize(cross(Output.NormalWS, Output.TangentWS) * tan.w);
	
	return Output;
}

PS_OUTPUT PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic)
{
	PS_OUTPUT Out = (PS_OUTPUT)0;

	Out.NormalMetallic = float4(0.5 * NormalVS + 0.5, metallic);
	Out.DiffuseRoughness = float4(BaseColor, roughness);
	Out.Emissive = float4(emissive.rgb, emissive.a / 256);
	return Out;
}

PS_OUTPUT GBufferPS(PS_INPUT In)
{
    Instance instanceData = GetInstanceData(PassCB.instanceId);
    Material materialData = GetMaterialData(instanceData.materialIdx);

	Texture2D txAlbedo = ResourceDescriptorHeap[materialData.diffuseIdx];
	Texture2D txNormal = ResourceDescriptorHeap[materialData.normalIdx];
	Texture2D txMetallicRoughness = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];
	Texture2D txEmissive = ResourceDescriptorHeap[materialData.emissiveIdx];

	float4 albedoColor = txAlbedo.Sample(LinearWrapSampler, In.Uvs) * float4(materialData.baseColorFactor, 1.0f);
#if MASK
	if (albedoColor.a < materialData.alphaCutoff) discard;
#endif

	float3 normal = normalize(In.NormalWS);
	if (In.IsFrontFace) normal = -normal; 

	float3 tangent = normalize(In.TangentWS);
	float3 bitangent = normalize(In.BitangentWS);
	float3 bumpMapNormal = txNormal.Sample(LinearWrapSampler, In.Uvs).xyz;
	bumpMapNormal = 2.0f * bumpMapNormal - 1.0f;
	float3x3 TBN = float3x3(tangent, bitangent, normal);
	float3 NewNormal = mul(bumpMapNormal, TBN);
	In.NormalVS = normalize(mul(NewNormal, (float3x3) FrameCB.view));

	float3 aoRoughnessMetallic = txMetallicRoughness.Sample(LinearWrapSampler, In.Uvs).rgb;

	float3 emissiveColor = txEmissive.Sample(LinearWrapSampler, In.Uvs).rgb;
	return PackGBuffer(albedoColor.xyz, normalize(In.NormalVS), float4(emissiveColor, materialData.emissiveFactor),
		aoRoughnessMetallic.g * materialData.roughnessFactor, aoRoughnessMetallic.b * materialData.metallicFactor);
}
