#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 Dir : POSITION;
};


float3 HosekWilkie(float cos_theta, float gamma, float cos_gamma)
{
    float3 A = weather_cbuf.A;
    float3 B = weather_cbuf.B;
    float3 C = weather_cbuf.C;
    float3 D = weather_cbuf.D;
    float3 E = weather_cbuf.E;
    float3 F = weather_cbuf.F;
    float3 G = weather_cbuf.G;
    float3 H = weather_cbuf.H;
    float3 I = weather_cbuf.I;
    
    float3 chi = (1 + cos_gamma * cos_gamma) / pow(1 + H * H - 2 * cos_gamma * H, float3(1.5f, 1.5f, 1.5f));
    return (1 + A * exp(B / (cos_theta + 0.01))) * (C + D * exp(E * gamma) + F * (cos_gamma * cos_gamma) + G * chi + I * sqrt(cos_theta));
}

float3 HosekWilkieSky(float3 v, float3 sun_dir)
{
    float cos_theta = clamp(v.y, 0, 1);
    float cos_gamma = clamp(dot(v, sun_dir), 0, 1);
    float gamma = acos(cos_gamma);

    float3 R = -weather_cbuf.Z * HosekWilkie(cos_theta, gamma, cos_gamma);
    return R;
}

[RootSignature(Sky_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
    float3 dir = normalize(pin.Dir);

    float3 col = HosekWilkieSky(dir, weather_cbuf.light_dir.xyz);

    return float4(col, 1.0);
    
}