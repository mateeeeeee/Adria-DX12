#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

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
ConstantBuffer<SSAOConstants> PassCB : register(b1);

struct SSAOKernel
{
    float4 samples[16];
};
ConstantBuffer<SSAOKernel> KernelCB : register(b2);

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void SSAO(CSInput input)
{
    Texture2D normalTx = ResourceDescriptorHeap[PassCB.normalIdx];
    Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
    Texture2D noiseTx = ResourceDescriptorHeap[PassCB.noiseIdx];
    RWTexture2D<float> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

    float2 ssaoParams = UnpackHalf2(PassCB.ssaoParamsPacked);
    float ssaoRadius = ssaoParams.x;
    float ssaoPower = ssaoParams.y;
    
    uint2 resolution = uint2(FrameCB.screenResolution) >> PassCB.resolutionFactor;
    float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / resolution;
    float3 viewNormal = normalTx.Sample(LinearBorderSampler, uv).rgb;
    viewNormal = 2.0f * viewNormal - 1.0f;
    viewNormal = normalize(viewNormal);
    
    float depth = depthTx.Sample(LinearBorderSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);
    float3 randomVector = normalize(2 * noiseTx.Sample(PointWrapSampler, uv * PassCB.noiseScale).xyz - 1);

    float3 tangent = normalize(randomVector - viewNormal * dot(randomVector, viewNormal));
    float3 bitangent = cross(viewNormal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, viewNormal);
    
    float occlusion = 0.0;
    [unroll(SSAO_KERNEL_SIZE)]
    for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        float3 sampleDir = mul(KernelCB.samples[i].xyz, TBN); 
        float3 samplePos = viewPosition + sampleDir * ssaoRadius;
        float4 offset = float4(samplePos, 1.0);
        offset = mul(offset, FrameCB.projection);
        offset.xy = ((offset.xy / offset.w) * float2(1.0f, -1.0f)) * 0.5f + 0.5f;
        float sampleDepth = depthTx.Sample(LinearBorderSampler, offset.xy);
        sampleDepth = GetViewPosition(offset.xy, sampleDepth).z;
        float rangeCheck = smoothstep(0.0, 1.0, ssaoRadius / abs(viewPosition.z - sampleDepth));
        occlusion += rangeCheck * step(sampleDepth, samplePos.z - 0.01);
    }
    occlusion = 1.0 - (occlusion / SSAO_KERNEL_SIZE);
    outputTx[input.DispatchThreadId.xy] = pow(abs(occlusion), ssaoPower);
}