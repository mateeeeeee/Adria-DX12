#include "Scene.hlsli"

struct GBufferConstants
{
    uint instanceId;
};
ConstantBuffer<GBufferConstants> PassCB : register(b1);

struct VSToPS
{
	float4 Position     : SV_POSITION;
	float3 PositionWS	: POSITION;
	float2 Uvs          : TEX;
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
	output.PositionWS = posWS.xyz;
	output.Position = mul(posWS, FrameCB.viewProjection);
	output.Position.xy += FrameCB.cameraJitter * output.Position.w;
	output.Uvs = uv;

	output.NormalWS =  mul(nor, (float3x3) transpose(instanceData.inverseWorldMatrix));
	output.TangentWS = mul(tan.xyz, (float3x3) instanceData.worldMatrix);
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

PSOutput GBufferPS(VSToPS input)
{
    Instance instanceData = GetInstanceData(PassCB.instanceId);
    Material materialData = GetMaterialData(instanceData.materialIdx);

	Texture2D albedoTx = ResourceDescriptorHeap[materialData.diffuseIdx];
	Texture2D normalTx = ResourceDescriptorHeap[materialData.normalIdx];
	Texture2D metallicRoughnessTx = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];
	Texture2D emissiveTx = ResourceDescriptorHeap[materialData.emissiveIdx];

	float4 albedoColor = albedoTx.Sample(LinearWrapSampler, input.Uvs) * float4(materialData.baseColorFactor, 1.0f);
	if (albedoColor.a < materialData.alphaCutoff) discard;

	float3 normal = normalize(input.NormalWS);
	float3 tangent = normalize(input.TangentWS);
	float3 bitangent = normalize(input.BitangentWS);
	float3x3 TBN = float3x3(tangent, bitangent, normal); 
	float3 normalTS = normalTx.Sample(LinearWrapSampler, input.Uvs).xyz;
	normalTS.xy = 2.0f * normalTS.xy - 1.0f;
	normalTS.z = sqrt(1.0f - normalTS.x * normalTS.x - normalTS.y * normalTS.y);
	normal = mul(normalTS, TBN);
	float3 aoRoughnessMetallic = metallicRoughnessTx.Sample(LinearWrapSampler, input.Uvs).rgb;

#if RAIN
	Texture3D rainSplashDiffuseTx = ResourceDescriptorHeap[FrameCB.rainSplashDiffuseIdx];
	Texture3D rainSplashBumpTx = ResourceDescriptorHeap[FrameCB.rainSplashBumpIdx];

	const float wetFactor = saturate(5.0f * saturate(input.NormalWS.y));
	float3 rainSplashDiffuse = rainSplashDiffuseTx.SampleLevel(LinearMirrorSampler, float3(input.PositionWS.xz / 5.0f, FrameCB.totalTime), 0).rgb;
	albedoColor.rgb += wetFactor * rainSplashDiffuse;

	float3 rainSplashBump = rainSplashBumpTx.SampleLevel(LinearMirrorSampler, float3(input.PositionWS.xz  / 10.0f, FrameCB.totalTime), 0).rgb - 0.5f;
	normal += wetFactor * 2 * (rainSplashBump.x * tangent + rainSplashBump.y * bitangent);

    albedoColor.rgb *= lerp(1.0f, 0.3f, wetFactor);
    aoRoughnessMetallic.g = saturate(lerp(aoRoughnessMetallic.g, aoRoughnessMetallic.g * 2.5f, wetFactor));
#endif
	float3 normalVS = normalize(mul(normal, (float3x3) FrameCB.view));

	float3 emissiveColor = emissiveTx.Sample(LinearWrapSampler, input.Uvs).rgb;
	return PackGBuffer(albedoColor.xyz, normalVS, float4(emissiveColor, materialData.emissiveFactor),
		aoRoughnessMetallic.g * materialData.roughnessFactor, aoRoughnessMetallic.b * materialData.metallicFactor);
}
