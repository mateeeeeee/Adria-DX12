#include "Tonemapping.hlsli"
#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct Bokeh
{
	float3 Position;
	float2 Size;
	float3 Color;
};

struct BokehGenerationConstants
{
	float  focusDistance;
	float  focusRadius;
	float  bokehLumThreshold;
	float  bokehBlurThreshold;
	float  bokehScale;
	float  bokehFallout;
};
ConstantBuffer<BokehGenerationConstants> BokehGenerationPassCB : register(b1);

struct BokehGenerationIndices
{
	uint hdrIdx;
	uint depthIdx;
	uint bokehStackIdx;
};
ConstantBuffer<BokehGenerationIndices> BokehGenerationBokehGenerationPassCB2 : register(b2);

float BlurFactor(in float depth)
{
	return saturate(abs(depth - BokehGenerationPassCB.focusDistance)) / BokehGenerationPassCB.focusRadius;
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BokehGenerationCS(CSInput input)
{
	Texture2D hdrTexture = ResourceDescriptorHeap[BokehGenerationBokehGenerationPassCB2.hdrIdx];
	Texture2D<float> depthTexture = ResourceDescriptorHeap[BokehGenerationBokehGenerationPassCB2.depthIdx];
	AppendStructuredBuffer<Bokeh> bokehBuffer = ResourceDescriptorHeap[BokehGenerationBokehGenerationPassCB2.bokehStackIdx];

	uint2 globalCoords = input.DispatchThreadId.xy;
	float2 uv = ((float2) globalCoords + 0.5f) * 1.0f / (FrameCB.displayResolution);

	float depth = depthTexture.Load(int3(globalCoords, 0));
	float centerDepth = LinearizeDepth(depth);

	if (depth < 1.0f)
	{
		float centerBlur = BlurFactor(centerDepth);
		float3 centerColor = hdrTexture.Load(int3(globalCoords, 0)).rgb;
		float3 averageColor = 0.0f;

		const uint NumSamples = 9;
		const uint2 SamplePoints[NumSamples] =
		{
			uint2(-1, -1), uint2(0, -1),  uint2(1, -1),
			uint2(-1,  0), uint2(0,  0),  uint2(1,  0),
			uint2(-1,  1), uint2(0,  1),  uint2(1,  1)
		};
		for (uint i = 0; i < NumSamples; ++i)
		{
			float3 hdrSample = hdrTexture.Load(int3(globalCoords + SamplePoints[i], 0)).rgb;
			averageColor += hdrSample;
		}
		averageColor /= NumSamples;

		float averageBrightness = dot(averageColor, 1.0f);
		float centerBrightness = dot(centerColor, 1.0f);
		float brightnessDiff = max(centerBrightness - averageBrightness, 0.0f);
		[branch]
		if (brightnessDiff >= BokehGenerationPassCB.bokehLumThreshold && centerBlur > BokehGenerationPassCB.bokehBlurThreshold)
		{
			Bokeh bPoint;
			bPoint.Position = float3(uv, centerDepth);
			bPoint.Size = centerBlur * BokehGenerationPassCB.bokehScale / FrameCB.displayResolution;

			float cocRadius = centerBlur * BokehGenerationPassCB.bokehScale * 0.45f;
			float cocArea = cocRadius * cocRadius * 3.14159f;
			float falloff = pow(saturate(1.0f / cocArea), BokehGenerationPassCB.bokehFallout);
			bPoint.Color = centerColor * falloff;
			bokehBuffer.Append(bPoint);
		}
	}
}
