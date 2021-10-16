#include "../Globals/GlobalsCS.hlsli"
#include "../Util/RootSignatures.hlsli"

#define COMPUTE_WORK_GROUP_DIM 32

static const float g = 9.81f;
static const float KM = 370.f;

Texture2D<float4> Phases            : register(t0);
Texture2D<float4> InitialSpectrum   : register(t1);
RWTexture2D<float4> Spectrum        : register(u0);

float2 multiplyComplex(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.y * b.x + a.x * b.y);
}

float2 multiplyByI(float2 z)
{
    return float2(-z.y, z.x);
}

float omega(float k)
{
    return sqrt(g * k * (1.0 + k * k / KM * KM));
}


[RootSignature(Spectrum_RS)]
[numthreads(COMPUTE_WORK_GROUP_DIM, COMPUTE_WORK_GROUP_DIM, 1)]
void cs_main(uint3 dispatchID : SV_DispatchThreadID)
{
    uint2 pixel_coord = dispatchID.xy;

    float n = (pixel_coord.x < 0.5f * uint(compute_cbuf.resolution)) ? pixel_coord.x : pixel_coord.x - float(compute_cbuf.resolution);
    float m = (pixel_coord.y < 0.5f * uint(compute_cbuf.resolution)) ? pixel_coord.y : pixel_coord.y - float(compute_cbuf.resolution);
    float2 wave_vector = (2.f * PI * float2(n, m)) / compute_cbuf.ocean_size;
                                                     
    float phase = Phases.Load(uint3(pixel_coord, 0)).r;
    
    float2 phase_vector = float2(cos(phase), sin(phase));
    uint2 coords = (uint2(compute_cbuf.resolution, compute_cbuf.resolution) - pixel_coord) % (compute_cbuf.resolution - 1);

    float2 h0 = float2(InitialSpectrum.Load(uint3(pixel_coord, 0)).r, 0.f);

    float2 h0Star = float2(InitialSpectrum.Load(uint3(coords, 0)).r, 0.f);
    h0Star.y *= -1.f;

    float2 h = multiplyComplex(h0, phase_vector) + multiplyComplex(h0Star, float2(phase_vector.x, -phase_vector.y));

    float2 hX = -multiplyByI(h * (wave_vector.x / length(wave_vector))) * compute_cbuf.ocean_choppiness;
    float2 hZ = -multiplyByI(h * (wave_vector.y / length(wave_vector))) * compute_cbuf.ocean_choppiness;

    // No DC term
    if (wave_vector.x == 0.0 && wave_vector.y == 0.0)
    {
        h  = float2(0.f, 0.f);
        hX = float2(0.f, 0.f);
        hZ = float2(0.f, 0.f);
    }
    
    Spectrum[pixel_coord] = float4(hX + multiplyByI(h), hZ);
}