#include "Scene.hlsli"
#include "Packing.hlsli"
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
	float4 NormalRT	: SV_TARGET0;
	float4 DiffuseRT : SV_TARGET1;
	float4 EmissiveRT : SV_TARGET2;
	float4 CustomRT	 : SV_TARGET3;
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

PSOutput GBufferPS(VSToPS input)
{
    Instance instanceData = GetInstanceData(GBufferPassCB.instanceId);
    Material materialData = GetMaterialData(instanceData.materialIdx);
	Texture2D albedoTexture = ResourceDescriptorHeap[materialData.diffuseIdx];
	Texture2D normalTexture = ResourceDescriptorHeap[materialData.normalIdx];
	Texture2D metallicRoughnessTexture = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];
	Texture2D emissiveTexture = ResourceDescriptorHeap[materialData.emissiveIdx];
	PSOutput output = (PSOutput)0;

#if VIEW_MIPMAPS
	static const float3 mipColors[6] = 
	{
		float3(1.0f, 0.0f, 0.0f),   // Mip 0: Red
		float3(0.0f, 1.0f, 0.0f),   // Mip 1: Green
		float3(0.0f, 0.0f, 1.0f),   // Mip 2: Blue
		float3(1.0f, 1.0f, 0.0f),   // Mip 3: Yellow
		float3(1.0f, 0.5f, 0.0f),   // Mip 4: Orange
		float3(0.5f, 0.0f, 0.5f)    // Mip 5 and above: Purple
	};
	float mipLevel = albedoTexture.CalculateLevelOfDetail(LinearWrapSampler, input.Uvs);
	mipLevel = clamp(mipLevel, 0.0f, 5.0f);
	int mipColorIndex = round(mipLevel);
	output.DiffuseRT = float4(mipColors[mipColorIndex], 1.0f);
	return output;
#endif
#if MATERIAL_ID
	const uint materialId = instanceData.materialIdx;
	output.DiffuseRT = float4(UintToColor(materialId), 1.0f);
	return output;
#endif
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

	float3 viewNormal = normalize(mul(normal, (float3x3) FrameCB.view));
	float4 emissive = float4(emissiveTexture.Sample(LinearWrapSampler, input.Uvs).rgb, materialData.emissiveFactor);
	float roughness = aoRoughnessMetallic.g * materialData.roughnessFactor;
	float metallic = aoRoughnessMetallic.b * materialData.metallicFactor;
	float4 customData = 0.0f;
	uint shadingExtension = ShadingExtension_Default;

#if SHADING_EXTENSION_ANISOTROPY
	shadingExtension = ShadingExtension_Anisotropy;
	float anisotropyStrength = materialData.anisotropyStrength;
	float anisotropyRotation = materialData.anisotropyRotation;
	float2 anisotropyDir = float2(1.0f, 0.5f);
	if (materialData.anisotropyIdx >= 0)
	{
		Texture2D anisotropyTexture = ResourceDescriptorHeap[materialData.anisotropyIdx];
		float3 anisotropyTexSample = anisotropyTexture.Sample(LinearWrapSampler, input.Uvs).rgb;
		anisotropyStrength *= anisotropyTexSample.b;
		anisotropyDir = anisotropyTexSample.rg;
	}
	anisotropyDir = 2.0f * anisotropyDir - 1.0f;
    float cosTheta = cos(anisotropyRotation);
    float sinTheta = sin(anisotropyRotation);
    float2 rotatedDir = float2(cosTheta * anisotropyDir.x - sinTheta * anisotropyDir.y,
							   sinTheta * anisotropyDir.x + cosTheta * anisotropyDir.y);
    float3 anisotropicT = normalize(mul(float3(rotatedDir, 0.0), TBN));
	customData = float4(anisotropicT, anisotropyStrength);
#elif SHADING_EXTENSION_CLEARCOAT
	shadingExtension = ShadingExtension_ClearCoat;
	float clearCoat = materialData.clearCoat;
	float clearCoatRoughness = materialData.clearCoatRoughness;
	
	Texture2D clearCoatTexture = ResourceDescriptorHeap[materialData.clearCoatIdx];
	Texture2D clearCoatRoughnessTexture = ResourceDescriptorHeap[materialData.clearCoatRoughnessIdx];
	clearCoat *= clearCoatTexture.Sample(LinearWrapSampler, input.Uvs).r;
	clearCoatRoughness *= clearCoatRoughnessTexture.Sample(LinearWrapSampler, input.Uvs).g;

	Texture2D clearCoatNormalTexture = ResourceDescriptorHeap[materialData.clearCoatNormalIdx];
    float3 clearCoatNormalTS = normalize(clearCoatNormalTexture.Sample(LinearWrapSampler, input.Uvs).xyz * 2.0f - 1.0f);
    float3 clearCoatNormal = normalize(mul(clearCoatNormalTS, TBN));
	float3 clearCoatNormalVS = normalize(mul(clearCoatNormal, (float3x3) FrameCB.view));
	customData = EncodeClearCoat(clearCoat, clearCoatRoughness, clearCoatNormalVS);
#elif SHADING_EXTENSION_SHEEN
	shadingExtension = ShadingExtension_Sheen;
	float3 sheenColor = materialData.sheenColor;
	float  sheenRoughness = materialData.sheenRoughness;
	Texture2D sheenColorTexture = ResourceDescriptorHeap[materialData.sheenColorIdx];
	Texture2D sheenRoughnessTexture = ResourceDescriptorHeap[materialData.sheenRoughnessIdx];
	sheenColor *= sheenColorTexture.Sample(LinearWrapSampler, input.Uvs).rgb;
	sheenRoughness *= sheenRoughnessTexture.Sample(LinearWrapSampler, input.Uvs).r;
	customData = float4(sheenColor, sheenRoughness);
#endif
	output.NormalRT = EncodeGBufferNormalRT(viewNormal, metallic, shadingExtension);
	output.DiffuseRT = float4(albedoColor.xyz * materialData.baseColorFactor, roughness);
	output.EmissiveRT = float4(emissive.rgb, emissive.a / 256);
	output.CustomRT = customData;

#if TRIANGLE_OVERDRAW
	RWTexture2D<uint> triangleOverdrawTexture = ResourceDescriptorHeap[FrameCB.triangleOverdrawIdx];
    uint2 texCoords = uint2(input.Position.xy);
	InterlockedAdd(triangleOverdrawTexture[texCoords], 1);
#endif
	return output;
}
