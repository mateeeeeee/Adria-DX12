#include "../Globals/GlobalsCS.hlsli"
#include "../Util/RootSignatures.hlsli"

static const float4 LUM_FACTOR = float4(0.299, 0.587, 0.114, 0);

Texture2D<float4> HDRTex   : register(t0);
Texture2D<float>  DepthTex : register(t1);

struct Bokeh
{
    float3 Position;
    float2 Size;
    float3 Color;
};

AppendStructuredBuffer<Bokeh> BokehStack : register(u0);

float BlurFactor(in float depth, in float4 DOF)
{
    float f0 = 1.0f - saturate((depth - DOF.x) / max(DOF.y - DOF.x, 0.01f));
    float f1 = saturate((depth - DOF.z) / max(DOF.w - DOF.z, 0.01f));
    float blur = saturate(f0 + f1);

    return blur;
}

float BlurFactor2(in float depth, in float2 DOF)
{
    return saturate((depth - DOF.x) / (DOF.y - DOF.x));
}

[RootSignature(BokehGenerate_RS)]
[numthreads(32, 32, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 CurPixel = dispatchThreadId.xy;

    uint width, height, levels;
    HDRTex.GetDimensions(0, width, height, levels);
    
    float2 uv = float2(CurPixel.x, CurPixel.y) / float2(width - 1, height - 1);
	
    float depth = DepthTex.Load(int3(CurPixel, 0));
    
    float centerDepth = ConvertZToLinearDepth(depth);
    
    if (depth < 1.0f) 
    {

        float centerBlur = BlurFactor(centerDepth, postprocess_cbuf.dof_params);

        const uint NumSamples = 9;
        const uint2 SamplePoints[NumSamples] =
        {
            uint2(-1, -1), uint2(0, -1),  uint2(1, -1),
		    uint2(-1,  0), uint2(0,  0),  uint2(1,  0),
		    uint2(-1,  1), uint2(0,  1),  uint2(1,  1)
        };

        
        float3 centerColor = HDRTex.Load(int3(CurPixel, 0)).rgb;
        
        float3 averageColor = 0.0f;
        for (uint i = 0; i < NumSamples; ++i)
        {
            float3 sample = HDRTex.Load(int3(CurPixel + SamplePoints[i], 0)).rgb;

            averageColor += sample;
        }
        averageColor /= NumSamples;

	    // Calculate the difference between the current texel and the average
        float averageBrightness = dot(averageColor, 1.0f);
        float centerBrightness  = dot(centerColor, 1.0f);
        float brightnessDiff    = max(centerBrightness - averageBrightness, 0.0f);

        [branch]
        if (brightnessDiff >= compute_cbuf.bokeh_lum_threshold && centerBlur > compute_cbuf.bokeh_blur_threshold)
        {
            Bokeh bPoint;
            bPoint.Position = float3(uv, centerDepth);
            bPoint.Size = centerBlur * compute_cbuf.bokeh_radius_scale / float2(width, height);
            
            float cocRadius = centerBlur * compute_cbuf.bokeh_radius_scale * 0.45f;
            float cocArea = cocRadius * cocRadius * 3.14159f;
            float falloff = pow(saturate(1.0f / cocArea), compute_cbuf.bokeh_fallout);
            bPoint.Color = centerColor * falloff;

            BokehStack.Append(bPoint);
        }


    }
    
}
