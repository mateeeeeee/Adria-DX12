#include "../Scene.hlsli"
#include "../Lighting.hlsli"

struct ShadowConstants
{
	uint  lightIndex;
	uint  matrixIndex;
};
ConstantBuffer<ShadowConstants> ShadowCB : register(b1);

struct ModelConstants
{
	uint instanceId;
};
ConstantBuffer<ModelConstants> ModelCB : register(b2);


struct VS_OUTPUT
{
	float4 Pos : SV_POSITION;
#if ALPHA_TEST
	float2 TexCoords : TEX;
#endif
};

VS_OUTPUT ShadowVS(uint vertexId : SV_VertexID)
{
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	StructuredBuffer<float4x4> lightViewProjections = ResourceDescriptorHeap[FrameCB.lightsMatricesIdx];
	Light light = lights[ShadowCB.lightIndex];
	float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + ShadowCB.matrixIndex];

	VS_OUTPUT output = (VS_OUTPUT)0;
	Instance instanceData = GetInstanceData(ModelCB.instanceId);
	Mesh meshData = GetMeshData(instanceData.meshIndex);

	float3 pos = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, vertexId);
	float4 posWS = mul(float4(pos, 1.0f), instanceData.worldMatrix);
	float4 posLS = mul(posWS, lightViewProjection);
	output.Pos = posLS;

#if ALPHA_TEST
	float2 uv = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, vertexId);
	output.TexCoords = uv;
#endif
	return output;
}

void ShadowPS(VS_OUTPUT input)
{
#if ALPHA_TEST 
	Instance instanceData = GetInstanceData(ModelCB.instanceId);
	Material materialData = GetMaterialData(instanceData.materialIdx);

	Texture2D albedoTx = ResourceDescriptorHeap[materialData.diffuseIdx];
	if (albedoTx.Sample(LinearWrapSampler, input.TexCoords).a < materialData.alphaCutoff) discard;
#endif
}
