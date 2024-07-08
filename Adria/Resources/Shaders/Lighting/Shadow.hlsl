#include "Scene.hlsli"
#include "Lighting.hlsli"

struct ShadowConstants
{
	uint  lightIndex;
	uint  matrixIndex;
};
ConstantBuffer<ShadowConstants> ShadowPassCB : register(b1);

struct ModelConstants
{
	uint instanceId;
};
ConstantBuffer<ModelConstants> ModelCB : register(b2);


struct VSToPS
{
	float4 Pos : SV_POSITION;
#if ALPHA_TEST
	float2 TexCoords : TEX;
#endif
};

VSToPS ShadowVS(uint VertexId : SV_VertexID)
{
	StructuredBuffer<Light> lightBuffer = ResourceDescriptorHeap[FrameCB.lightsIdx];
	StructuredBuffer<float4x4> lightViewProjections = ResourceDescriptorHeap[FrameCB.lightsMatricesIdx];
	Light light = lightBuffer[ShadowPassCB.lightIndex];
	float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + ShadowPassCB.matrixIndex];

	VSToPS output = (VSToPS)0;
	Instance instanceData = GetInstanceData(ModelCB.instanceId);
	Mesh meshData = GetMeshData(instanceData.meshIndex);

	float3 pos = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, VertexId);
	float4 posWS = mul(float4(pos, 1.0f), instanceData.worldMatrix);
	float4 posLS = mul(posWS, lightViewProjection);
	output.Pos = posLS;

#if ALPHA_TEST
	float2 uv = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, VertexId);
	output.TexCoords = uv;
#endif
	return output;
}

void ShadowPS(VSToPS input)
{
#if ALPHA_TEST 
	Instance instanceData = GetInstanceData(ModelCB.instanceId);
	Material materialData = GetMaterialData(instanceData.materialIdx);

	Texture2D albedoTexture = ResourceDescriptorHeap[materialData.diffuseIdx];
	if (albedoTexture.Sample(LinearWrapSampler, input.TexCoords).a < materialData.alphaCutoff) discard;
#endif
}
