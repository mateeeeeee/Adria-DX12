#ifndef  _BRDF_
#define  _BRDF_

#include "Constants.hlsli"
#include "CommonResources.hlsli"



#define ROUGHNESS_CUTOFF 0.7
#define GGX_SAMPLE_VISIBLE
#define PI 3.1415
#define GGX_IMPORTANCE_SAMPLE_BIAS 0.1
#define MIN_ROUGHNESS (0.03)

float Lambert(float3 normal, float3 lightIncident)
{
    return max(0, dot(normal, lightIncident)) * M_PI_INV;
}
float3 DiffuseBRDF(float3 diffuse)
{
    return diffuse * M_PI_INV;
}

float D_GGX(float3 N, float3 H, float a)
{
    float a2 = clamp(a * a, 0.0001f, 1.0f);
    float NdotH = saturate(dot(N, H));
    
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;
    
    return a2 * rcp(max(denom, 0.001f));
}
float D_GGX_Aniso(float ax, float ay, float3 T, float3 B, float3 N, float3 H)
{
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    
    float a2 = ax * ay;
    float3 d = float3(ay * TdotH, ax * BdotH, a2 * NdotH);
    float d2 = dot(d, d);
    float b2 = a2 / d2;
    return a2 * b2 * b2 * M_PI_INV;
}
//http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float V_SmithGGX(float3 N, float3 V, float3 L, float a)
{
    float a2 = clamp(a * a, 0.0001f, 1.0f);
    float NdotV = saturate(abs(dot(N, V)) + 1e-5); 
    float NdotL = saturate(dot(N, L));

    float G_V = NdotV + sqrt((NdotV - NdotV * a2) * NdotV + a2);
    float G_L = NdotL + sqrt((NdotL - NdotL * a2) * NdotL + a2);
    return rcp(G_V * G_L);
}

float V_SmithJointApprox(float3 N, float3 V, float3 L, float a)
{
    float a2 = clamp(a * a, 0.0001f, 1.0f);
    float NdotV = saturate(abs(dot(N, V)) + 1e-5); 
    float NdotL = saturate(dot(N, L));

	float V_SmithV = NdotL * (NdotV * (1 - a2) + a2);
	float V_SmithL = NdotV * (NdotL * (1 - a2) + a2);
	return 0.5 * rcp(V_SmithV + V_SmithL);
}


float V_SmithGGX_Aniso(float ax, float ay, float3 T, float3 B, float3 N, float3 V, float3 L)
{
    float TdotV = dot(T, V);
    float TdotL = dot(T, L);
    float BdotV = dot(B, V);
    float BdotL = dot(B, L);
    float NdotV = dot(N, V);
    float NdotL = dot(N, L);
    
    float G_V = NdotL * length(float3(ax * TdotV, ay * BdotV, NdotV));
    float G_L = NdotV * length(float3(ax * TdotL, ay * BdotL, NdotL));
    return 0.5 * rcp(G_V + G_L);
}
float3 F_Schlick(float3 V, float3 H, float3 F0)
{
    float VdotH = saturate(dot(V, H) + 1e-5);
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}
float F_Schlick(float VdotH, float F0)
{
    return F0 + (1 - F0) * pow(max(1 - VdotH, 0), 5);
}

float3 SpecularBRDF(float3 N, float3 V, float3 L, float3 specular, float roughness, out float3 F)
{
    roughness = max(roughness, MIN_ROUGHNESS);

    float a = roughness * roughness;
    float3 H = normalize(V + L);

    float D = D_GGX(N, H, a);
    float Vis = V_SmithGGX(N, V, L, a);
    F = F_Schlick(V, H, specular);

    return D * Vis * F;
}
float3 DefaultBRDF(float3 L, float3 V, float3 N, float3 diffuse, float3 specular, float roughness)
{
    float3 F;
    float3 specularBrdf = SpecularBRDF(N, V, L, specular, roughness, F);
    float3 diffuseBrdf = DiffuseBRDF(diffuse) * (1.0 - F);

    return diffuseBrdf + specularBrdf;
}

// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_sheen/README.md
float L(float x, float alphaG)
{
    float OneMinusAlphaSq = (1.0 - alphaG) * (1.0 - alphaG);
    float a = lerp(21.5473, 25.3245, OneMinusAlphaSq);
    float b = lerp(3.82987, 3.32435, OneMinusAlphaSq);
    float c = lerp(0.19823, 0.16801, OneMinusAlphaSq);
    float d = lerp(-1.97760, -1.27393, OneMinusAlphaSq);
    float e = lerp(-4.32054, -4.85967, OneMinusAlphaSq);
    return a / (1.0 + b * pow(x, c)) + d * x + e;
}
float LambdaSheen(float cosTheta, float alphaG)
{
    return abs(cosTheta) < 0.5 ? exp(L(cosTheta, alphaG)) : exp(2.0 * L(0.5, alphaG) - L(1.0 - cosTheta, alphaG));
}
float3 SheenBRDF(float3 L, float3 V, float3 N, float3 sheenColor, float sheenRoughness)
{
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));

    sheenRoughness = max(sheenRoughness, MIN_ROUGHNESS);
    float alphaG = sheenRoughness * sheenRoughness;
    float invR = 1.0 / alphaG;
    float cos2h = NdotH * NdotH;
    float sin2h = 1.0 - cos2h;

    float sheenDistribution = (2.0 + invR) * pow(sin2h, 0.5 * invR) / (2.0 * M_PI);

    float lambdaV = LambdaSheen(NdotV, alphaG);
    float lambdaL = LambdaSheen(NdotL, alphaG);
    //Charlie model
    float sheenVisibility = 1.0f / ((1.0f + lambdaV + lambdaL) * (4.0f * NdotV * NdotL));
    //Ashikhmin and Premoze
    //float sheenVisibility = 1.0f / (4.0f * (NdotL + NdotV - NdotL * NdotV));
    return sheenColor * sheenDistribution * sheenVisibility;
}
float SheenScale(float3 V, float3 N, float3 sheenColor, float sheenRoughness, Texture2D sheenETexture)
{
    float VdotN = saturate(dot(V, N));
    float maxSheen = max(max(sheenColor.r, sheenColor.g), sheenColor.b);
    float E_VdotN = sheenETexture.SampleLevel(LinearWrapSampler, float2(VdotN, sheenRoughness * sheenRoughness), 0).r;
    return 1.0f - maxSheen * E_VdotN;
}

float DielectricSpecularToF0(float specular)
{
    return 0.08f * specular;
}
float3 ComputeF0(float specular, float3 baseColor, float metalness)
{
    return lerp(DielectricSpecularToF0(specular).xxx, baseColor, metalness);
}
float3 ComputeDiffuseColor(float3 baseColor, float metalness)
{
    return baseColor * (1 - metalness);
}

struct BrdfData
{
    float3 Diffuse;
    float3 Specular;
    float  Roughness;
};


BrdfData GetBrdfData(float3 materialAlbedo, float metallic, float roughness)
{
	BrdfData data	= (BrdfData)0;
    data.Diffuse	= ComputeDiffuseColor(materialAlbedo, metallic);
    data.Specular	= ComputeF0(0.5f, materialAlbedo, metallic);
    data.Roughness	= roughness;
    return data;
}

float2 SampleDisk(float2 random)
{
    float angle = 2 * M_PI * random.x;
    return float2(cos(angle), sin(angle)) * sqrt(random.y);
}
float3 SampleCosHemisphere(float2 random, out float solidAnglePdf)
{
    float2 tangential = SampleDisk(random);
    float elevation = sqrt(saturate(1.0 - random.y));
    solidAnglePdf = elevation / M_PI;
    return float3(tangential.xy, elevation);
}

// Brian Karis, Epic Games "Real Shading in Unreal Engine 4"
float4 ImportanceSampleGGX(float2 Xi, float Roughness)
{
	float m = Roughness * Roughness;
	float m2 = m * m;

	float Phi = 2 * PI * Xi.x;

	float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (m2 - 1.0) * Xi.y));
	float SinTheta = sqrt(max(1e-5, 1.0 - CosTheta * CosTheta));

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float d = (CosTheta * m2 - CosTheta) * CosTheta + 1;
	float D = m2 / (PI * d * d);
	float pdf = D * CosTheta;

	return float4(H, pdf);
}


// [ Duff et al. 2017, "Building an Orthonormal Basis, Revisited" ]
// http://jcgt.org/published/0006/01/01/
float3x3 GetTangentBasis(float3 TangentZ)
{
	const float Sign = TangentZ.z >= 0 ? 1 : -1;
	const float a = -rcp(Sign + TangentZ.z);
	const float b = TangentZ.x * TangentZ.y * a;

	float3 TangentX = { 1 + Sign * a * pow(TangentZ.x, 2), Sign * b, -Sign * TangentZ.x };
	float3 TangentY = { b, Sign + a * pow(TangentZ.y, 2), -TangentZ.y };

	return float3x3(TangentX, TangentY, TangentZ);
}

// Adapted from: "Sampling the GGX Distribution of Visible Normals", by E. Heitz
// http://jcgt.org/published/0007/04/01/paper.pdf
float4 ImportanceSampleVisibleGGX(float2 diskXi, float roughness, float3 V)
{
	float alphaRoughness = roughness * roughness;
	float alphaRoughnessSq = alphaRoughness * alphaRoughness;

	// Transform the view direction to hemisphere configuration
	float3 Vh = normalize(float3(alphaRoughness * V.xy, V.z));

	// Orthonormal basis
	// tangent0 is orthogonal to N.
	float3 tangent0 = (Vh.z < 0.9999) ? normalize(cross(float3(0, 0, 1), Vh)) : float3(1, 0, 0);
	float3 tangent1 = cross(Vh, tangent0);

	float2 p = diskXi;
	float s = 0.5 + 0.5 * Vh.z;
	p.y = (1 - s) * sqrt(1 - p.x * p.x) + s * p.y;

	// Reproject onto hemisphere
	float3 H;
	H = p.x * tangent0;
	H += p.y * tangent1;
	H += sqrt(saturate(1 - dot(p, p))) * Vh;

	// Transform the normal back to the ellipsoid configuration
	H = normalize(float3(alphaRoughness * H.xy, max(0.0, H.z)));

	float NdotV = V.z;
	float NdotH = H.z;
	float VdotH = dot(V, H);

	// Microfacet Distribution
	float f = (NdotH * alphaRoughnessSq - NdotH) * NdotH + 1;
	float D = alphaRoughnessSq / (PI * f * f);

	// Smith Joint masking function
	float SmithGGXMasking = 2.0 * NdotV / (sqrt(NdotV * (NdotV - NdotV * alphaRoughnessSq) + alphaRoughnessSq) + NdotV);

	// D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
	float PDF = SmithGGXMasking * VdotH * D / NdotV;

	return float4(H, PDF);
}

float4 ReflectionDir_GGX(float3 V, float3 N, float roughness, float2 random2)
{
	float4 H;
	float3 L;
	if (roughness > 0.05f)
	{
		float3x3 tangentBasis = GetTangentBasis(N);
		float3 tangentV = mul(tangentBasis, V);
		float2 Xi = random2;
		Xi.y = lerp(Xi.y, 0.0f, GGX_IMPORTANCE_SAMPLE_BIAS);
		H = ImportanceSampleVisibleGGX(SampleDisk(Xi), roughness, tangentV);
		H.xyz = mul(H.xyz, tangentBasis);
		L = reflect(-V, H.xyz);
	}
	else
	{
		H = float4(N.xyz, 1.0f);
		L = reflect(-V, H.xyz);
	}
	float PDF = H.w;
	return float4(L, PDF);
}

#endif