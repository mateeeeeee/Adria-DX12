#include "Constants.hlsli"

float3 Diffuse_Lambert(float3 diffuseColor)
{
    return diffuseColor * M_PI_INV;
}

float Smith_G1_GGX(float alpha, float NdotS, float alphaSquared, float NdotSSquared)
{
    return 2.0f / (sqrt(((alphaSquared * (1.0f - NdotSSquared)) + NdotSSquared) / NdotSSquared) + 1.0f);
}

float Smith_G2_Over_G1_Height_Correlated(float alpha, float alphaSquared, float NdotL, float NdotV)
{
    float G1V = Smith_G1_GGX(alpha, NdotV, alphaSquared, NdotV * NdotV);
    float G1L = Smith_G1_GGX(alpha, NdotL, alphaSquared, NdotL * NdotL);
    return G1L / (G1V + G1L - G1V * G1L);
}

float Vis_SmithJointApprox(float a2, float NdotV, float NdotL)
{
    float Vis_SmithV = NdotL * (NdotV * (1 - a2) + a2);
    float Vis_SmithL = NdotV * (NdotL * (1 - a2) + a2);
    return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}

float D_GGX(float a2, float NdotH)
{
    float d = (NdotH * a2 - NdotH) * NdotH + 1;
    return a2 / (M_PI * d * d);
}

float3 F_Schlick(float3 f0, float VdotH)
{
    float Fc = pow(1.0f - VdotH, 5);
    return Fc + (1.0f - Fc) * f0;
}

float3 F_Schlick(float3 f0, float3 f90, float VdotH)
{
    float Fc = pow(1.0f - VdotH, 5);
    return f90 * Fc + (1.0f - Fc) * f0;
}

float3 SampleGGXVNDF(float3 V, float2 alpha2D, float2 u)
{
	float3 Vh = normalize(float3(alpha2D.x * V.x, alpha2D.y * V.y, V.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0.0f ? float3(-Vh.y, Vh.x, 0.0f) * rsqrt(lensq) : float3(1.0f, 0.0f, 0.0f);
    float3 T2 = cross(Vh, T1);
    float r = sqrt(u.x);
    float phi = 2 * M_PI * u.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5f * (1.0f + Vh.z);
    t2 = lerp(sqrt(1.0f - t1 * t1), t2, s);
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;
    return normalize(float3(alpha2D.x * Nh.x, alpha2D.y * Nh.y, max(0.0f, Nh.z)));
}

float3 EnvDFGPolynomial(float3 specularColor, float gloss, float ndotv)
{
    float x = gloss;
    float y = ndotv;

    float b1 = -0.1688;
    float b2 = 1.895;
    float b3 = 0.9903;
    float b4 = -4.853;
    float b5 = 8.404;
    float b6 = -5.069;
    float bias = saturate(min(b1 * x + b2 * x * x, b3 + b4 * y + b5 * y * y + b6 * y * y * y));

    float d0 = 0.6045;
    float d1 = 1.699;
    float d2 = -0.5228;
    float d3 = -3.603;
    float d4 = 1.404;
    float d5 = 0.1939;
    float d6 = 2.661;
    float delta = saturate(d0 + d1 * x + d2 * y + d3 * x * x + d4 * x * y + d5 * y * y + d6 * x * x * x);
    float scale = delta - bias;

    bias *= saturate(50.0 * specularColor.y);
    return specularColor * scale + bias;
}