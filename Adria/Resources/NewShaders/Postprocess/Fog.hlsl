#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

#define BLOCK_SIZE 16
#define EXPONENTIAL_FOG 0
#define EXPONENTIAL_HEIGHT_FOG 1

struct FogConstants
{
    float fogFalloff;
    float fogDensity;
    float fogStart;
    uint  fogColor;
    uint  fogType;
    
    uint  depthIdx;
    uint  sceneIdx;
    uint  outputIdx;
};
ConstantBuffer<FogConstants> PassCB : register(b1);

float ExponentialFog(float dist)
{
    float fogDist = max(dist - PassCB.fogStart, 0.0);
    float fog = exp(-fogDist * PassCB.fogDensity);
    return 1 - fog;
}
float ExponentialFog(float4 viewPosition)
{
    float4 worldPosition = mul(viewPosition, FrameCB.inverseView);
    worldPosition /= worldPosition.w;
    float3 cameraToWorld = (worldPosition - FrameCB.cameraPosition).xyz;
    float distance = length(cameraToWorld);
    float fogDist = max(distance - PassCB.fogStart, 0.0);
    float fog = exp(-fogDist * PassCB.fogDensity);
    return 1 - fog;
}
float ExponentialHeightFog(float4 viewPosition)
{
    float4 worldPosition = mul(viewPosition, FrameCB.inverseView);
    worldPosition /= worldPosition.w;
    float3 cameraToWorld = (worldPosition - FrameCB.cameraPosition).xyz;
    float distance = length(cameraToWorld);
    float fogDist = max(distance - PassCB.fogStart, 0.0);
    
	// Distance based fog intensity
    float fogHeightDensityAtViewer = exp(-PassCB.fogFalloff * FrameCB.cameraPosition.y);
    float fogDistInt = fogDist * fogHeightDensityAtViewer;

	// Height based fog intensity
    float eyeToPixelY = cameraToWorld.y * (fogDist / distance);
    float t = PassCB.fogFalloff * eyeToPixelY;
    const float thresholdT = 0.01;
    float fogHeightInt = abs(t) > thresholdT ? (1.0 - exp(-t)) / t : 1.0;
	// Combine both factors to get the final factor
    float fog = exp(-PassCB.fogDensity * fogDistInt * fogHeightInt);
    return 1 - fog;
}

struct CS_INPUT
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void Fog(CS_INPUT input)
{
    Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
    Texture2D sceneTx = ResourceDescriptorHeap[PassCB.sceneIdx];
    RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
    
    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);
    
    float4 mainColor = sceneTx.Sample(LinearWrapSampler, uv);
    float depth = depthTx.Sample(LinearWrapSampler, uv);
    float3 pos_vs = GetViewPosition(uv, depth);

    float fog = 0.0f;
    if (PassCB.fogType == EXPONENTIAL_FOG)
    {
        fog = ExponentialFog(float4(pos_vs, 1.0f));
    }
    else if (PassCB.fogType == EXPONENTIAL_HEIGHT_FOG)
    {
        fog = ExponentialHeightFog(float4(pos_vs, 1.0f));
    }
    outputTx[input.DispatchThreadId.xy] = lerp(mainColor, UnpackUintColor(PassCB.fogColor), fog);
}