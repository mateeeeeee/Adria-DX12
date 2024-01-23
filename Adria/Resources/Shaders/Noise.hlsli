#ifndef _NOISE_
#define _NOISE_

// Hash by David_Hoskins
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uint2(UI0, UI1)
#define UI3 uint3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))

float3 Hash33(float3 p)
{
	uint3 q = uint3(int3(p)) * UI3;
	q = (q.x ^ q.y ^ q.z) * UI3;
	return -1. + 2. * float3(q) * UIF;
}

float Remap(float x, float a, float b, float c, float d)
{
	return (((x - a) / (b - a)) * (d - c)) + c;
}

float GradientNoise(float3 x, float freq)
{
	// grid
	float3 p = floor(x);
	float3 w = frac(x);

	// quintic interpolant
	float3 u = w * w * w * (w * (w * 6. - 15.) + 10.);

	// gradients
	float3 ga = Hash33(fmod(p + float3(0., 0., 0.), freq));
	float3 gb = Hash33(fmod(p + float3(1., 0., 0.), freq));
	float3 gc = Hash33(fmod(p + float3(0., 1., 0.), freq));
	float3 gd = Hash33(fmod(p + float3(1., 1., 0.), freq));
	float3 ge = Hash33(fmod(p + float3(0., 0., 1.), freq));
	float3 gf = Hash33(fmod(p + float3(1., 0., 1.), freq));
	float3 gg = Hash33(fmod(p + float3(0., 1., 1.), freq));
	float3 gh = Hash33(fmod(p + float3(1., 1., 1.), freq));

	// projections
	float va = dot(ga, w - float3(0., 0., 0.));
	float vb = dot(gb, w - float3(1., 0., 0.));
	float vc = dot(gc, w - float3(0., 1., 0.));
	float vd = dot(gd, w - float3(1., 1., 0.));
	float ve = dot(ge, w - float3(0., 0., 1.));
	float vf = dot(gf, w - float3(1., 0., 1.));
	float vg = dot(gg, w - float3(0., 1., 1.));
	float vh = dot(gh, w - float3(1., 1., 1.));

	// interpolation
	return va +
		u.x * (vb - va) +
		u.y * (vc - va) +
		u.z * (ve - va) +
		u.x * u.y * (va - vb - vc + vd) +
		u.y * u.z * (va - vc - ve + vg) +
		u.z * u.x * (va - vb - ve + vf) +
		u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
}


float WorleyNoise(float3 uv, float freq)
{
	float3 id = floor(uv);
	float3 p = frac(uv);

	float min_dist = 10000.;
	for (float x = -1.; x <= 1.; ++x)
	{
		for (float y = -1.; y <= 1.; ++y)
		{
			for (float z = -1.; z <= 1.; ++z)
			{
				float3 offset = float3(x, y, z);
				float3 h = Hash33(fmod(id + offset, float3(freq, freq, freq))) * .5 + .5;
				h += offset;
				float3 d = p - h;
				min_dist = min(min_dist, dot(d, d));
			}
		}
	}

	return 1. - min_dist;
}

float PerlinFBM(float3 p, float freq, int octaves)
{
	float G = exp2(-.85);
	float amp = 1.;
	float noise = 0.;
	for (int i = 0; i < octaves; ++i)
	{
		noise += amp * GradientNoise(p * freq, freq);
		freq *= 2.;
		amp *= G;
	}

	return noise;
}

// Tileable Worley fbm inspired by Andrew Schneider's Real-Time Volumetric Cloudscapes
// chapter in GPU Pro 7.
float WorleyFBM(float3 p, float freq)
{
	return WorleyNoise(p * freq, freq) * .625 +
		   WorleyNoise(p * freq * 2., freq * 2.) * .25 +
		   WorleyNoise(p * freq * 4., freq * 4.) * .125;
}


uint3 PCG3D16(uint3 v)
{
    v = v * 12829u + 47989u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v >>= 16u;
    return v;
}

// Simplex noise, transforms given position onto triangle grid
float2 Simplex(float2 P)
{
    const float F2 = (sqrt(3.0) - 1.0) / 2.0;  // 0.36602540378
    const float G2 = (3.0 - sqrt(3.0)) / 6.0;  // 0.2113248654
    float   u   = (P.x + P.y) * F2;
    float2 Pi   = round(P + u);
    float  v    = (Pi.x + Pi.y) * G2;
    float2 P0   = Pi - v;  

    return P - P0;  
}


#endif