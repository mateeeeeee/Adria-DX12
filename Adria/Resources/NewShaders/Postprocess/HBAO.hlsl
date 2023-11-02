#include "../CommonResources.hlsli"
#include "../Constants.hlsli"

#define BLOCK_SIZE 16
#define HBAO_NUM_DIRECTIONS 8
#define HBAO_NUM_STEPS 4

static const float NdotVBias = 0.1f;

struct HBAOConstants
{
    float r2;
    float radiusToScreen;
    float power;
    float noiseScale;
    
    uint depthIdx;
    uint normalIdx;
    uint noiseIdx;
    uint outputIdx;
};
ConstantBuffer<HBAOConstants> PassCB : register(b1);

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

float Falloff(float distanceSquare)
{
    return distanceSquare * (-1.0f / PassCB.r2) + 1.0;
}
float ComputeAO(float3 p, float3 n, float3 s)
{
    float3 v = s - p;
    float VdotV = dot(v, v);
    float NdotV = dot(n, v) * 1.0 / sqrt(VdotV);
    return clamp(NdotV - NdotVBias, 0, 1) * clamp(Falloff(VdotV), 0, 1);
}
float2 RotateDirection(float2 dir, float2 cosSin)
{
    return float2(dir.x * cosSin.x - dir.y * cosSin.y,
                  dir.x * cosSin.y + dir.y * cosSin.x);
}
float ComputeCoarseAO(Texture2D<float> depthTx, float2 UV, float radiusInPixels, float3 rand, float3 viewPosition, float3 viewNormal)
{
    float stepSizeInPixels = radiusInPixels / (HBAO_NUM_STEPS + 1);
    const float theta = 2.0 * M_PI / HBAO_NUM_DIRECTIONS;
    float AO = 0;

    for (float directionIndex = 0; directionIndex < HBAO_NUM_DIRECTIONS; ++directionIndex)
    {
        float angle = theta * directionIndex;
        float2 direction = RotateDirection(float2(cos(angle), sin(angle)), rand.xy);
        float rayT = (rand.z * stepSizeInPixels + 1.0);
        for (float stepIndex = 0; stepIndex < HBAO_NUM_STEPS; ++stepIndex)
        {
            float2 SnappedUV = round(rayT * direction) / FrameCB.displayResolution + UV;
            float depth = depthTx.Sample(LinearBorderSampler, SnappedUV);
            float3 S = GetViewPosition(SnappedUV, depth);
            rayT += stepSizeInPixels;
            AO += ComputeAO(viewPosition, viewNormal, S);
        }
    }
    AO *= (1.0f / (1.0f - NdotVBias)) / (HBAO_NUM_DIRECTIONS * HBAO_NUM_STEPS);
    return clamp(1.0 - AO * 2.0, 0, 1);
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void HBAO(CSInput input)
{
    Texture2D normalTx = ResourceDescriptorHeap[PassCB.normalIdx];
    Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
    Texture2D noiseTx = ResourceDescriptorHeap[PassCB.noiseIdx];
    RWTexture2D<float> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
    
    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
    float depth = depthTx.Sample(LinearBorderSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);

    float3 viewNormal = normalTx.Sample(LinearBorderSampler, uv).rgb;
    viewNormal = 2.0f * viewNormal - 1.0f;
    viewNormal = normalize(viewNormal);
    float radius_in_pixels = PassCB.radiusToScreen / viewPosition.z;
    float3 rand = noiseTx.Sample(PointWrapSampler, uv * PassCB.noiseScale).xyz;

    float AO = ComputeCoarseAO(depthTx, uv, radius_in_pixels, rand, viewPosition, viewNormal);
    outputTx[input.DispatchThreadId.xy] = pow(AO, PassCB.power);
}