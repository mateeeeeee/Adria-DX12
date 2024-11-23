#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIRUtil.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct IntialSamplingConstants
{
    uint depthIdx;
    uint prevDepthIdx;
    uint normalIdx;
    uint reservoirBufferIdx;
};
ConstantBuffer<IntialSamplingConstants> IntialSamplingCB : register(b1);

[numthreads(16, 16, 1)]
void InitialSamplingCS( uint3 dispatchThreadID : SV_DispatchThreadID )
{
    Texture2D<float>                        depthTexture     = ResourceDescriptorHeap[IntialSamplingCB.depthIdx];
    Texture2D<float3>                       normalTexture    = ResourceDescriptorHeap[IntialSamplingCB.normalIdx];
    RWStructuredBuffer<ReSTIR_DI_Reservoir> reservoirBuffer  = ResourceDescriptorHeap[IntialSamplingCB.reservoirBufferIdx];
    
    float depth = depthTexture[dispatchThreadID.xy].r;
    float3 N = normalTexture[dispatchThreadID.xy].rgb;
    N = 2.0f * N - 1.0f;
	N = normalize(mul(N, (float3x3) transpose(FrameCB.view)));

    if (depth == 0.0f)
    {
        outputRadiance[dispatchThreadID.xy] = 0.xxxx;
        outputRayDirection[dispatchThreadID.xy] = 0;
        return;
    }

    uint randSeed = InitRand(dispatchThreadID.x + dispatchThreadID.y * 16, 0, 16);
    float2 randFloat2 = float2(NextRand(randSeed), NextRand(randSeed));
    float3 direction = GetCosHemisphereSample(randSeed, N);
    float3 worldPos = GetWorldPosition(FullScreenPosition(dispatchThreadID.xy), depth);

    RayDesc ray;
    ray.Origin = worldPos + N * 0.01f;
    ray.Direction = direction;
    ray.TMin = 0.00001f;
    ray.TMax = 10000.0f;

    ReSTIR_DI_Reservoir reservoir;
    reservoir.Reset();

    float3 radiance = 0.0f;
    float3 hitNormal = 0.0f;
    float3 hitPosition = 0.0f;
    HitInfo info = (HitInfo)0;
    if (TraceRay(ray, info))
    {
        Instance instanceData = GetInstanceData(info.instanceIndex);
		Mesh meshData = GetMeshData(instanceData.meshIndex);
		Material materialData = GetMaterialData(instanceData.materialIdx);

		VertexData vertex = LoadVertexData(meshData, info.primitiveIndex, info.barycentricCoordinates);
        
        float3 worldPosition = mul(vertex.pos, info.objectToWorldMatrix).xyz;
        float3 worldNormal = normalize(mul(vertex.nor, (float3x3) transpose(info.worldToObjectMatrix)));
        float3 geometryNormal = normalize(worldNormal);
        float3 V = -ray.Direction;
        MaterialProperties matProperties = GetMaterialProperties(materialData, vertex.uv, 0);
        BrdfData brdfData = GetBrdfData(matProperties);
        
        //#todo
        float3 directLighting = 0.0f; 
        float3 indirectLighting = 0.0f; //GetIndirectDiffuseLighting(info.hitPosition, matProperties);

        radiance = (directLighting + indirectLighting + matProperties.emissive);
        hitNormal = worldNormal;
        hitPosition = worldPosition;
    }
    else
    {
        TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
        radiance = envMap.SampleLevel(LinearWrapSampler, ray.Direction, 0).rgb;
        hitNormal = -direction;
        hitPosition = 0.0f;
    }

    reservoir.sample.samplePosition = hitPosition;
    reservoir.sample.sampleNormal = hitNormal;
    reservoir.sample.sampleRadiance = radiance;
    reservoirBuffer[dispatchThreadID.x] = reservoir;
}