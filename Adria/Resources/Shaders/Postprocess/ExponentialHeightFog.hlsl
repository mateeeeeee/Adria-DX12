#include "CommonResources.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16

struct ExponentialHeightFogConstants
{
    float fogFalloff;
    float fogDensity;
	float fogHeight;
    float fogStart;

	float fogAtViewPosition;
	float fogMinOpacity;
	float fogCutoffDistance;
    uint  fogColor;

    uint  depthIdx;
    uint  sceneIdx;
    uint  outputIdx;
};
ConstantBuffer<ExponentialHeightFogConstants> PassCB : register(b2);



float4 CalculateExponentialHeightFog(float4 viewPosition)
{
	float4 worldPosition = mul(viewPosition, FrameCB.inverseView);
	worldPosition /= worldPosition.w;

	float4 cameraPosition = FrameCB.cameraPosition;

	float3 cameraToPos = (worldPosition - cameraPosition).xyz;
	float distance = length(cameraToPos);
	float fogStartDistance = PassCB.fogStart;
	float3 cameraToPosNormalized = cameraToPos / distance;

	float rayOriginTerms = PassCB.fogAtViewPosition;
	float rayLength = distance;
	float rayDirectionY = cameraToPos.y;
	if (fogStartDistance > 0)
	{
		float excludeIntersectionTime = fogStartDistance / distance;
		float cameraToExclusionIntersectionY = excludeIntersectionTime * cameraToPos.y;
		float exclusionIntersectionY = cameraPosition.y + cameraToExclusionIntersectionY;
		rayLength = (1.0f - excludeIntersectionTime) * distance;
		rayDirectionY = cameraToPos.y - cameraToExclusionIntersectionY;
		float exponent = PassCB.fogFalloff * (exclusionIntersectionY - PassCB.fogHeight);
		rayOriginTerms = PassCB.fogDensity * exp2(-exponent);
	}

	float falloff = max(-127.0f, PassCB.fogFalloff * rayDirectionY);
	float lineIntegral = (1.0f - exp2(-falloff)) / falloff;
	float lineIntegralTaylor = log(2.0f) - (0.5f * pow(log(2.0f), 2)) * falloff;
	float exponentialHeightLineIntegralCalc = rayOriginTerms * (abs(falloff) > 0.01f ? lineIntegral : lineIntegralTaylor);
	float exponentialHeightLineIntegral = exponentialHeightLineIntegralCalc * rayLength;
	float expFogFactor = max(saturate(exp2(-exponentialHeightLineIntegral)), PassCB.fogMinOpacity);

	float3 inscatteringColor = UnpackUintColor(PassCB.fogColor).rgb;

	if (PassCB.fogCutoffDistance > 0 && distance > PassCB.fogCutoffDistance)
	{
		expFogFactor = 1;
	}

	return float4(inscatteringColor * (1.0f - expFogFactor), expFogFactor);
}

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ExponentialHeightFogCS(CSInput input)
{
    Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
    Texture2D sceneTx = ResourceDescriptorHeap[PassCB.sceneIdx];
    RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
    
    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
    
    float depth = depthTx.Sample(LinearWrapSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);

    float4 fog = CalculateExponentialHeightFog(float4(viewPosition, 1.0f));

	float4 mainColor = sceneTx.Sample(LinearWrapSampler, uv);
    float4 result = mainColor;
    result.rgb += fog.a * fog.rgb;
    outputTx[input.DispatchThreadId.xy] = result;
}