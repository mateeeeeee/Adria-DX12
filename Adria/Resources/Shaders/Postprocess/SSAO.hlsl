#include "CommonResources.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16
#define SSAO_KERNEL_SIZE 16

struct SSAOConstants
{
    uint ssaoParamsPacked;
    uint resolutionFactor;
    float2 noiseScale;
    
    uint depthIdx;
    uint normalIdx;
    uint noiseIdx;
    uint outputIdx;
};
ConstantBuffer<SSAOConstants> SSAOPassCB : register(b1);

struct SSAOKernel
{
    float4 samples[16];
};
ConstantBuffer<SSAOKernel> SSAOKernelCB : register(b2);

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void SSAO_CS(CSInput input)
{
    Texture2D normalTexture = ResourceDescriptorHeap[SSAOPassCB.normalIdx];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[SSAOPassCB.depthIdx];
    Texture2D noiseTexture = ResourceDescriptorHeap[SSAOPassCB.noiseIdx];
    RWTexture2D<float> outputTexture = ResourceDescriptorHeap[SSAOPassCB.outputIdx];

    float2 ssaoParams = UnpackHalf2(SSAOPassCB.ssaoParamsPacked);
    float ssaoRadius = ssaoParams.x;
    float ssaoPower = ssaoParams.y;
    
    uint2 resolution = uint2(FrameCB.renderResolution) >> SSAOPassCB.resolutionFactor;
    float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / resolution;
    float3 viewNormal = normalTexture.Sample(LinearBorderSampler, uv).rgb;
    viewNormal = 2.0f * viewNormal - 1.0f;
    viewNormal = normalize(viewNormal);
    
    float depth = depthTexture.Sample(LinearBorderSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);
    float3 randomVector = normalize(2 * noiseTexture.Sample(PointWrapSampler, uv * SSAOPassCB.noiseScale).xyz - 1);

    float3 tangent = normalize(randomVector - viewNormal * dot(randomVector, viewNormal));
    float3 bitangent = cross(viewNormal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, viewNormal);
    
    float occlusion = 0.0;
    [unroll(SSAO_KERNEL_SIZE)]
    for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        float3 sampleDir = mul(SSAOKernelCB.samples[i].xyz, TBN); 
        float3 samplePos = viewPosition + sampleDir * ssaoRadius;
        float4 offset = float4(samplePos, 1.0);
        offset = mul(offset, FrameCB.projection);
        offset.xy = ((offset.xy / offset.w) * float2(1.0f, -1.0f)) * 0.5f + 0.5f;
        float sampleDepth = depthTexture.Sample(LinearBorderSampler, offset.xy);
        sampleDepth = GetViewPosition(offset.xy, sampleDepth).z;
        float rangeCheck = smoothstep(0.0, 1.0, ssaoRadius / abs(viewPosition.z - sampleDepth));
        occlusion += rangeCheck * step(sampleDepth, samplePos.z - 0.01);
    }
    occlusion = 1.0 - (occlusion / SSAO_KERNEL_SIZE);
    outputTexture[input.DispatchThreadId.xy] = pow(abs(occlusion), ssaoPower);
}