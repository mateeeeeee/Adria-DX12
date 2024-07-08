#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Constants.hlsli"
#include "Random.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"

struct ReSTIRConstants
{
    uint depthIdx;
    uint prevDepthIdx;
    uint normalIdx;
    uint irradianceHistoryIdx;
    uint outputIrradianceIdx;
    uint outputRayDirectionIdx;
};
ConstantBuffer<ReSTIRConstants> ReSTIRPassCB : register(b1);


float3 GetIndirectDiffuseLighting(float3 position, MaterialProperties materialProperties)
{
    if (ReSTIRPassCB.irradianceHistoryIdx < 0) //#todo make int, not uint
    {
        return 0.0;
    }
    Texture2D historyIrradianceTexture = ResourceDescriptorHeap[ReSTIRPassCB.irradianceHistoryIdx];
    Texture2D prevDepthTexture = ResourceDescriptorHeap[ReSTIRPassCB.prevDepthIdx];
    
    float4 prevClipPos = mul(float4(position, 1.0), FrameCB.prevViewProjection);
    float3 prevNdcPos = prevClipPos.xyz / prevClipPos.w;
    float2 prevUV = prevNdcPos.xy * float2(0.5, -0.5) + 0.5;
    float prevLinearDepth = LinearizeDepth(prevDepthTexture.SampleLevel(PointClampSampler, prevUV, 0.0).x);
    
    if (any(prevUV < 0.0) || any(prevUV > 1.0) || abs(LinearizeDepth(prevNdcPos.z) - prevLinearDepth) > 0.05)
    {
        return 0.0;
    }
    float3 irradiance = historyIrradianceTexture.SampleLevel(LinearWrapSampler, prevUV, 0).xyz;
    return irradiance * materialProperties.baseColor;
}

[numthreads(16, 16, 1)]
void InitialSamplingCS( uint3 dispatchThreadID : SV_DispatchThreadID )
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[ReSTIRPassCB.depthIdx];
    Texture2D<float3> normalTexture = ResourceDescriptorHeap[ReSTIRPassCB.normalIdx];
    RWTexture2D<float4> outputRadiance = ResourceDescriptorHeap[ReSTIRPassCB.outputIrradianceIdx];
    RWTexture2D<uint> outputRayDirection = ResourceDescriptorHeap[ReSTIRPassCB.outputRayDirectionIdx];
    StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
    
    float depth = depthTexture[dispatchThreadID.xy].r;
    float3 N = normalTexture[dispatchThreadID.xy].rgb;
    N = 2.0 * N - 1.0;
	N = normalize(mul(N, (float3x3) transpose(FrameCB.view)));

    if (depth == 0.0)
    {
        outputRadiance[dispatchThreadID.xy] = 0.xxxx;
        outputRayDirection[dispatchThreadID.xy] = 0;
        return;
    }
    float3 worldPos = GetWorldPosition(FullScreenPosition(dispatchThreadID.xy), depth);
    
    uint randSeed = InitRand(dispatchThreadID.x + dispatchThreadID.y * 16, 0, 16);
    float2 randFloat2 = float2(NextRand(randSeed), NextRand(randSeed));
    float3 direction = GetCosHemisphereSample(randSeed, N);

    RayDesc ray;
    ray.Origin = worldPos + N * 0.01;
    ray.Direction = direction;
    ray.TMin = 0.00001;
    ray.TMax = 10000.0;

    float3 radiance = 0.0f;
    float3 hitNormal = 0.0f;
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
        

        //#todo use all lights
        Light light = lights[0];
		float visibility = TraceShadowRay(light, worldPosition.xyz);
        float3 wi = normalize(-light.direction.xyz);
        float3 wo = normalize(FrameCB.cameraPosition.xyz - worldPosition);
		float NdotL = saturate(dot(worldNormal, wi));

        float3 directLighting = DefaultBRDF(wi, wo, worldNormal, brdfData.Diffuse, brdfData.Specular, brdfData.Roughness) * visibility * light.color.rgb * NdotL;
        float3 indirectLighting = GetIndirectDiffuseLighting(info.hitPosition, matProperties);

        radiance = (directLighting + indirectLighting + matProperties.emissive);
        hitNormal = worldNormal;
    }
    else
    {
        TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
        radiance = envMap.SampleLevel(LinearWrapSampler, ray.Direction, 0).rgb;
        hitNormal = -direction;
    }

    outputRadiance[dispatchThreadID.xy] = float4(radiance, info.hitT);
    outputRayDirection[dispatchThreadID.xy] = EncodeNormal16x2(direction);
}