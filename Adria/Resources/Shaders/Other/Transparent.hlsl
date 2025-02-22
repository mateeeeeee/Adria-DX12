
#include "Scene.hlsli"
#include "Packing.hlsli"
#if USE_SSR
#include "Postprocess/SSRCommon.hlsli"
#endif
//#if RAIN
//#include "Weather/RainUtil.hlsli"
//#endif

struct TransparentConstants
{
    uint instanceId;
	uint sceneIdx;
	uint depthIdx;
};
ConstantBuffer<TransparentConstants> TransparentPassCB : register(b1);

struct VSToPS
{
	float4 Position     : SV_POSITION;
	float3 PositionWS	: POSITION;
	float2 Uvs          : TEX;
	float3 TangentWS    : TANGENT;
	float3 BitangentWS  : BITANGENT;
	float3 NormalWS     : NORMAL1;
};


VSToPS TransparentVS(uint vertexId : SV_VertexID)
{
	VSToPS output = (VSToPS)0;

    Instance instanceData = GetInstanceData(TransparentPassCB.instanceId);
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

float4 TransparentPS(VSToPS input) : SV_TARGET
{
	Instance instanceData = GetInstanceData(TransparentPassCB.instanceId);
    Material materialData = GetMaterialData(instanceData.materialIdx);
	Texture2D albedoTexture = ResourceDescriptorHeap[materialData.diffuseIdx];
	Texture2D normalTexture = ResourceDescriptorHeap[materialData.normalIdx];
	float4 color = albedoTexture.Sample(LinearWrapSampler, input.Uvs) * float4(materialData.baseColorFactor, 1.0f);
#if USE_SSR
	Texture2D<float> depthTexture = ResourceDescriptorHeap[TransparentPassCB.depthIdx];
	Texture2D sceneTexture = ResourceDescriptorHeap[TransparentPassCB.sceneIdx];

	float3 position = input.PositionWS;
	float3 normal = normalize(input.NormalWS);
	float3 tangent = normalize(input.TangentWS);
	float3 bitangent = normalize(input.BitangentWS);
    float3 normalTS = normalize(normalTexture.Sample(LinearWrapSampler, input.Uvs).xyz * 2.0f - 1.0f);
    float3x3 TBN = float3x3(tangent, bitangent, normal); 
    normal = normalize(mul(normalTS, TBN));
	//Add normal map
	float roughness = materialData.roughnessFactor;

	float4 viewPosition = mul(float4(position, 1.0f), FrameCB.view); viewPosition /= viewPosition.w;
	float3 viewNormal = normalize(mul(normal, (float3x3) FrameCB.view));		

	float3 reflectDir = normalize(reflect(viewPosition.xyz, viewNormal));
    float4 coords = SSRRayMarch(depthTexture, reflectDir, 1.6f, 2.0f, viewPosition.xyz);

    float2 coordsEdgeFactors = float2(1, 1) - pow(saturate(abs(coords.xy - float2(0.5f, 0.5f)) * 2), 8);
    float  screenEdgeFactor = saturate(min(coordsEdgeFactors.x, coordsEdgeFactors.y));

    float3 hitColor = sceneTexture.SampleLevel(LinearClampSampler, coords.xy, 0).rgb;
    float roughnessMask = saturate(1.0f - (roughness / 0.7f));
    roughnessMask *= roughnessMask;
    float4 fresnel = clamp(pow(1 - dot(normalize(viewPosition.xyz), viewNormal), 1), 0, 1);

    float4 reflectionColor = float4(saturate(hitColor.rgb * screenEdgeFactor * roughnessMask), 1.0f);
	color += fresnel * max(0, reflectionColor);
#endif
	return float4(color.rgb, 0.5f);
}