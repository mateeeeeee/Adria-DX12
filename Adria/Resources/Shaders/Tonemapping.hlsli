#ifndef _TONEMAPPING_
#define _TONEMAPPING_

#define Tonemap_None 0
#define Tonemap_Reinhard 1
#define Tonemap_Hable 2
#define Tonemap_Linear 3
#define Tonemap_TonyMcMapface 4
#define Tonemap_AgX 5
#define Tonemap_ACES 6

static const float gamma = 2.2;

static float Luminance(float3 color)
{
	return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}

static float3 LinearToSRGB(float3 linearRGB)
{
	return pow(linearRGB, 1.0f / gamma);
}

static float3 LinearToneMapping(float3 color)
{
	color = clamp(color, 0., 1.);
	color = LinearToSRGB(color);
	return color;
}
static float3 ReinhardToneMapping(float3 color)
{
	float luma = Luminance(color);
	float toneMappedLuma = luma / (1. + luma);
	if (luma > 1e-6)
		color *= toneMappedLuma / luma;

	color = LinearToSRGB(color);
	return color;
}
static float3 ACESFilmicToneMapping(float3 color)
{
	color *= 0.6f;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	color = (color * (a * color + b)) / (color * (c * color + d) + e);
	color = LinearToSRGB(color);
	return clamp(color, 0.0f, 1.0f);
}


static float3 HableToneMapping(float3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	float exposure = 2.;
	color *= exposure;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	color = LinearToSRGB(color);
	return color;
}

//https://github.com/h3r2tic/tony-mc-mapface/blob/main/shader/tony_mc_mapface.hlsl
float3 TonyMcMapface(Texture3D<float3> LUT, SamplerState LinearClampSampler, float3 color) 
{
    const float3 encoded = color / (color + 1.0);
    const float LUT_DIMS = 48.0;
    const float3 uv = encoded * ((LUT_DIMS - 1.0) / LUT_DIMS) + 0.5 / LUT_DIMS;
    float3 result = LUT.SampleLevel(LinearClampSampler, uv, 0);
	result = LinearToSRGB(result);
	return result;
}

//https://iolite-engine.com/blog_posts/minimal_agx_implementation
//https://github.com/sobotka/AgX
// 0: Default, 1: Golden, 2: Punchy
#define AGX_LOOK 2

float3 AgXDefaultContrastApprox(float3 x) 
{
    float3 x2 = x * x;
    float3 x4 = x2 * x2;
    return + 15.5     * x4 * x2
           - 40.14    * x4 * x
           + 31.96    * x4
           - 6.868    * x2 * x
           + 0.4298   * x2
           + 0.1191   * x
           - 0.00232;
}

float3 AgX(float3 Color) 
{
	const float3x3 AgXTransform = float3x3(
											0.842479062253094, 0.0423282422610123, 0.0423756549057051,
											0.0784335999999992,  0.878468636469772,  0.0784336,
											0.0792237451477643, 0.0791661274605434, 0.879142973793104);

    const float MinEv = -12.47393f;
    const float MaxEv = 4.026069f; 

    Color = mul(AgXTransform, Color);
    Color = clamp(log2(Color), MinEv, MaxEv);
    Color = (Color - MinEv) / (MaxEv - MinEv);
    return AgXDefaultContrastApprox(Color);
}

float3 AgXEotf(float3 Color) 
{
	const float3x3 InvAgXTransform = float3x3(
											  1.19687900512017, -0.0528968517574562, -0.0529716355144438,
											  -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
											  -0.0990297440797205, -0.0989611768448433, 1.15107367264116);
    
    Color = mul(InvAgXTransform, Color);
	return Color;
}


float3 AgXLook(float3 val) 
{
  const float3 lw = float3(0.2126, 0.7152, 0.0722);
  float luma = dot(val, lw);
  
  // Default
  float3 offset = 0.0;
  float3 slope = 1.0;
  float3 power = 1.0;
  float sat = 1.0;
 
#if AGX_LOOK == 1
  // Golden
  slope = float3(1.0, 0.9, 0.5);
  power = 0.8;
  sat = 0.8;
#elif AGX_LOOK == 2
  // Punchy
  slope = 1.0;
  power = 1.35;
  sat = 1.4;
#endif
  
  val = pow(val * slope + offset, power);
  return luma + sat * (val - luma);
}

static float3 AgXToneMapping(float3 color)
{
	color = AgX(color);
	color = AgXLook(color); 
	color = AgXEotf(color);
	return color;
}

#endif