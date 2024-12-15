#include "Scene.hlsli"
#if RAIN
#include "Weather/RainUtil.hlsli"
#endif

struct GBufferConstants
{
    uint instanceId;
};
ConstantBuffer<GBufferConstants> GBufferPassCB : register(b1);

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
	float4 NormalMetallic	: SV_TARGET0;
	float4 DiffuseRoughness : SV_TARGET1;
	float4 Emissive			: SV_TARGET2;
	float4 Custom			: SV_TARGET3;

};

VSToPS GBufferVS(uint vertexId : SV_VertexID)
{
	VSToPS output = (VSToPS)0;

    Instance instanceData = GetInstanceData(GBufferPassCB.instanceId);
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

PSOutput PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic, uint extension, float3 customData)
{
	PSOutput output = (PSOutput)0;
	output.NormalMetallic = float4(0.5f * NormalVS + 0.5f, metallic);
	output.DiffuseRoughness = float4(BaseColor, roughness);
	output.Emissive = float4(emissive.rgb, emissive.a / 256);
	output.Custom = float4(customData, (float)extension / 255.0f);
	return output;
}

PSOutput GBufferPS(VSToPS input)
{
    Instance instanceData = GetInstanceData(GBufferPassCB.instanceId);
    Material materialData = GetMaterialData(instanceData.materialIdx);

	Texture2D albedoTexture = ResourceDescriptorHeap[materialData.diffuseIdx];
	Texture2D normalTexture = ResourceDescriptorHeap[materialData.normalIdx];
	Texture2D metallicRoughnessTexture = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];
	Texture2D emissiveTexture = ResourceDescriptorHeap[materialData.emissiveIdx];

	float4 albedoColor = albedoTexture.Sample(LinearWrapSampler, input.Uvs) * float4(materialData.baseColorFactor, 1.0f);
	if (albedoColor.a < materialData.alphaCutoff) discard;

	float3 normal = normalize(input.NormalWS);
	float3 tangent = normalize(input.TangentWS);
	float3 bitangent = normalize(input.BitangentWS);
    float3 normalTS = normalize(normalTexture.Sample(LinearWrapSampler, input.Uvs).xyz * 2.0f - 1.0f);
    float3x3 TBN = float3x3(tangent, bitangent, normal); 
    normal = normalize(mul(normalTS, TBN));

	float3 aoRoughnessMetallic = metallicRoughnessTexture.Sample(LinearWrapSampler, input.Uvs).rgb;
#if RAIN
	ApplyRain(input.PositionWS.xyz, albedoColor.rgb, aoRoughnessMetallic.g, normal, tangent, bitangent);
#endif
	float3 normalVS = normalize(mul(normal, (float3x3) FrameCB.view));

	uint extension = 0;
	float3 customData = 0.0f;
#if SHADING_EXTENSION_ANISOTROPY
	//#todo
#elif SHADING_EXTENSION_CLEARCOAT
	//#todo
#endif
	float3 emissiveColor = emissiveTexture.Sample(LinearWrapSampler, input.Uvs).rgb;
	return PackGBuffer(albedoColor.xyz * materialData.baseColorFactor, normalVS, float4(emissiveColor, materialData.emissiveFactor),
					   aoRoughnessMetallic.g * materialData.roughnessFactor, aoRoughnessMetallic.b * materialData.metallicFactor, extension, customData);
}
