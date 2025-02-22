
#include "Scene.hlsli"
#include "Packing.hlsli"
//#if RAIN
//#include "Weather/RainUtil.hlsli"
//#endif

struct TransparentConstants
{
    uint instanceId;
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
	float4 albedoColor = albedoTexture.Sample(LinearWrapSampler, input.Uvs) * float4(materialData.baseColorFactor, 0.5f);
	return albedoColor;
}