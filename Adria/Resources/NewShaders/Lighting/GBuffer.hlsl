#include "../Scene.hlsli"

struct GBufferConstants
{
    uint instanceId;
};
ConstantBuffer<GBufferConstants> PassCB : register(b1);

struct VSToPS
{
	float4 Position     : SV_POSITION;

	float2 Uvs          : TEX;
	float3 NormalVS     : NORMAL0;
	float3 TangentWS    : TANGENT;
	float3 BitangentWS  : BITANGENT;
	float3 NormalWS     : NORMAL1;
};

struct PSOutput
{
	float4 NormalMetallic : SV_TARGET0;
	float4 DiffuseRoughness : SV_TARGET1;
	float4 Emissive : SV_TARGET2;
};

VSToPS GBufferVS(uint vertexId : SV_VertexID)
{
	VSToPS output = (VSToPS)0;

    Instance instanceData = GetInstanceData(PassCB.instanceId);
    Mesh meshData = GetMeshData(instanceData.meshIndex);

	float3 pos = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, vertexId);
	float2 uv  = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, vertexId);
	float3 nor = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, vertexId);
	float4 tan = LoadMeshBuffer<float4>(meshData.bufferIdx, meshData.tangentsOffset, vertexId);
    
	float4 posWS = mul(float4(pos, 1.0), instanceData.worldMatrix);
	output.Position = mul(posWS, FrameCB.viewProjection);
	output.Position.xy += FrameCB.cameraJitter * output.Position.w;

	output.Uvs = uv;

	float3 normalWS = mul(nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
	output.NormalWS = normalWS;
	output.NormalVS = mul(normalWS, (float3x3) transpose(FrameCB.inverseView));
	output.TangentWS = mul(tan.xyz, (float3x3) transpose(instanceData.inverseWorldMatrix));
	output.BitangentWS = normalize(cross(output.NormalWS, output.TangentWS) * tan.w);
	
	return output;
}

PSOutput PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic)
{
	PSOutput output = (PSOutput)0;
	output.NormalMetallic = float4(0.5 * NormalVS + 0.5, metallic);
	output.DiffuseRoughness = float4(BaseColor, roughness);
	output.Emissive = float4(emissive.rgb, emissive.a / 256);
	return output;
}

PSOutput GBufferPS(VSToPS input, bool isFrontFace : SV_IsFrontFace)
{
    Instance instanceData = GetInstanceData(PassCB.instanceId);
    Material materialData = GetMaterialData(instanceData.materialIdx);

	Texture2D txAlbedo = ResourceDescriptorHeap[materialData.diffuseIdx];
	Texture2D txNormal = ResourceDescriptorHeap[materialData.normalIdx];
	Texture2D txMetallicRoughness = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];
	Texture2D txEmissive = ResourceDescriptorHeap[materialData.emissiveIdx];

	float4 albedoColor = txAlbedo.Sample(LinearWrapSampler, input.Uvs) * float4(materialData.baseColorFactor, 1.0f);
#if MASK
	if (albedoColor.a < materialData.alphaCutoff) discard;
#endif

	float3 normal = normalize(input.NormalWS);
	if (isFrontFace) normal = -normal; 

	float3 tangent = normalize(input.TangentWS);
	float3 bitangent = normalize(input.BitangentWS);
	float3 bumpMapNormal = txNormal.Sample(LinearWrapSampler, input.Uvs).xyz;
	bumpMapNormal = 2.0f * bumpMapNormal - 1.0f;
	float3x3 TBN = float3x3(tangent, bitangent, normal);
	float3 NewNormal = mul(bumpMapNormal, TBN);
	input.NormalVS = normalize(mul(NewNormal, (float3x3) FrameCB.view));

	float3 aoRoughnessMetallic = txMetallicRoughness.Sample(LinearWrapSampler, input.Uvs).rgb;

	float3 emissiveColor = txEmissive.Sample(LinearWrapSampler, input.Uvs).rgb;
	return PackGBuffer(albedoColor.xyz, normalize(input.NormalVS), float4(emissiveColor, materialData.emissiveFactor),
		aoRoughnessMetallic.g * materialData.roughnessFactor, aoRoughnessMetallic.b * materialData.metallicFactor);
}
