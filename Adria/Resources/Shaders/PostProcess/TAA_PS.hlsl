#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

//https://github.com/NVIDIAGameWorks/Falcor/blob/master/Source/RenderPasses/Antialiasing/TAA/TAA.ps.slang

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};

SamplerState linear_wrap_sampler : register(s0);

Texture2D hdr_scene : register(t0);
Texture2D prev_hdr_scene : register(t1);
//Texture2D velocity_buffer : register(t2);  for dynamic scenes, see MotionBlurPS on how to generate it

static const float gAlpha = 0.1f;
static const float gColorBoxSigma = 1.0f;

// Catmull-Rom filtering code from http://vec3.ca/bicubic-filtering-in-fewer-taps/
float3 BicubicSampleCatmullRom(Texture2D tex, float2 sample_pos, float2 tex_dim)
{
    float2 tex_dim_inv = 1.0 / tex_dim;
    float2 tc = floor(sample_pos - 0.5) + 0.5;
    float2 f = sample_pos - tc;
    float2 f2 = f * f;
    float2 f3 = f2 * f;

    float2 w0 = f2 - 0.5 * (f3 + f);
    float2 w1 = 1.5 * f3 - 2.5 * f2 + 1;
    float2 w3 = 0.5 * (f3 - f2);
    float2 w2 = 1 - w0 - w1 - w3;

    float2 w12 = w1 + w2;
    
    float2 tc0 = (tc - 1) * tex_dim_inv;
    float2 tc12 = (tc + w2 / w12) * tex_dim_inv;
    float2 tc3 = (tc + 2) * tex_dim_inv;

    float3 result =
        tex.SampleLevel(linear_wrap_sampler, float2(tc0.x, tc0.y), 0).rgb * (w0.x * w0.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc0.x, tc12.y), 0).rgb * (w0.x * w12.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc0.x, tc3.y), 0).rgb * (w0.x * w3.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc12.x, tc0.y), 0).rgb * (w12.x * w0.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc12.x, tc12.y), 0).rgb * (w12.x * w12.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc12.x, tc3.y), 0).rgb * (w12.x * w3.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc3.x, tc0.y), 0).rgb * (w3.x * w0.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc3.x, tc12.y), 0).rgb * (w3.x * w12.y) +
        tex.SampleLevel(linear_wrap_sampler, float2(tc3.x, tc3.y), 0).rgb * (w3.x * w3.y);

    return result;
}

/** Converts color from RGB to YCgCo space
    \param RGBColor linear HDR RGB color
*/
inline float3 RGBToYCgCo(float3 rgb)
{
    float Y = dot(rgb, float3(0.25f, 0.50f, 0.25f));
    float Cg = dot(rgb, float3(-0.25f, 0.50f, -0.25f));
    float Co = dot(rgb, float3(0.50f, 0.00f, -0.50f));
    return float3(Y, Cg, Co);
}

/** Converts color from YCgCo to RGB space
    \param YCgCoColor linear HDR YCgCo color
*/
inline float3 YCgCoToRGB(float3 YCgCo)
{
    float tmp = YCgCo.x - YCgCo.y;
    float r = tmp + YCgCo.z;
    float g = YCgCo.x + YCgCo.y;
    float b = tmp - YCgCo.z;
    return float3(r, g, b);
}

[RootSignature(Taa_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
    const int2 offset[8] =
    {
        int2(-1, -1), int2(-1, 1),
        int2(1, -1), int2(1, 1),
        int2(1, 0), int2(0, -1),
        int2(0, 1), int2(-1, 0),
    };
    
    uint2 tex_dim;
    uint levels;
    hdr_scene.GetDimensions(0, tex_dim.x, tex_dim.y, levels);

    float2 pos = pin.Tex * tex_dim;
    int2 ipos = int2(pos);

    // Fetch the current pixel color and compute the color bounding box
    // Details here: http://www.gdcvault.com/play/1023521/From-the-Lab-Bench-Real
    // and here: http://cwyman.org/papers/siga16_gazeTrackedFoveatedRendering.pdf
    float3 color = hdr_scene.Load(int3(ipos, 0)).rgb;
    color = RGBToYCgCo(color);
    float3 colorAvg = color;
    float3 colorVar = color * color;
    [unroll]
    for (int k = 0; k < 8; k++)
    {
        float3 c = hdr_scene.Load(int3(ipos + offset[k], 0)).rgb;
        c = RGBToYCgCo(c);
        colorAvg += c;
        colorVar += c * c;
    }

    float oneOverNine = 1.0 / 9.0;
    colorAvg *= oneOverNine;
    colorVar *= oneOverNine;

    float3 sigma = sqrt(max(0.0f, colorVar - colorAvg * colorAvg));
    float3 colorMin = colorAvg - gColorBoxSigma * sigma;
    float3 colorMax = colorAvg + gColorBoxSigma * sigma;

    // Find the longest motion vector
    //float2 motion = gTexMotionVec.Load(int3(ipos, 0)).xy;
    //[unroll]
    //for (int a = 0; a < 8; a++)
    //{
    //    float2 m = gTexMotionVec.Load(int3(ipos + offset[a], 0)).rg;
    //    motion = dot(m, m) > dot(motion, motion) ? m : motion;
    //}
    //
    // Use motion vector to fetch previous frame color (history)
    float3 history = BicubicSampleCatmullRom(prev_hdr_scene, pin.Tex * tex_dim, tex_dim);

    history = RGBToYCgCo(history);

    // Anti-flickering, based on Brian Karis talk @Siggraph 2014
    // https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
    // Reduce blend factor when history is near clamping
    float distToClamp = min(abs(colorMin.x - history.x), abs(colorMax.x - history.x));
    float alpha = clamp((gAlpha * distToClamp) / (distToClamp + colorMax.x - colorMin.x), 0.0f, 1.0f);

    history = clamp(history, colorMin, colorMax);
    float3 result = YCgCoToRGB(lerp(history, color, alpha));
    return float4(result, 0);
}

