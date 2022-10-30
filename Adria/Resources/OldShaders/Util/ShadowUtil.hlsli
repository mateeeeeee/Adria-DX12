
float CalcShadowFactor_Basic(SamplerComparisonState shadow_sampler,
	Texture2D shadow_map,
	float3 uvd)
{
    if (uvd.z > 1.0f)
        return 1.0;
	
    return shadow_map.SampleCmpLevelZero(shadow_sampler,
		uvd.xy, uvd.z).r;
}


float CalcShadowFactor_PCF3x3(SamplerComparisonState samShadow,
	Texture2D shadowMap,
	float3 uvd, int smSize, float softness)
{
    if (uvd.z > 1.0f)
        return 1.0;

    float depth = uvd.z;

    const float dx = 1.0f / smSize;

    float percentLit = 0.0f;

    float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

	[unroll]
    for (int i = 0; i < 9; ++i)
    {
        offsets[i] = offsets[i] * float2(softness, softness);
        percentLit += shadowMap.SampleCmpLevelZero(samShadow,
			uvd.xy + offsets[i], depth).r;
    }
    return percentLit /= 9.0f;

}

//CSM
//////////////////////////////////////////////////////////

float CSMCalcShadowFactor_Basic(SamplerComparisonState samShadow,
	Texture2DArray shadowMap, uint index,
	float3 uvd, int smSize, float softness)
{
    return shadowMap.SampleCmpLevelZero(samShadow,
		float3(uvd.xy, index), uvd.z).r;
}

float CSMCalcShadowFactor_PCF3x3(SamplerComparisonState samShadow,
	Texture2DArray shadowMap, uint index,
	float3 uvd, int smSize, float softness)
{
    const float dx = 1.0f / smSize;

    float percentLit = 0.0f;

    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };


	[unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += shadowMap.SampleCmpLevelZero(samShadow,
			float3(uvd.xy + offsets[i], index), uvd.z).r;
    }
    return percentLit /= 9.0f;

}


//////////////////////////////////////////////////////////


float CalcShadowFactor_PCF5x5(SamplerComparisonState samShadow,
	Texture2D shadowMap,
	float3 shadowPosH, int smSize, float softness)
{
    if (shadowPosH.z > 1.0f)
        return 1.0;
	
    float depth = shadowPosH.z;

    const float dx = 1.0f / smSize;

    float percentLit = 0.0f;
    float2 offsets[25] =
    {
        float2(-2 * dx, -2 * dx), float2(-1 * dx, -2 * dx), float2(0 * dx, -2 * dx), float2(1 * dx, -2 * dx), float2(2 * dx, -2 * dx),
		float2(-2 * dx, -1 * dx), float2(-1 * dx, -1 * dx), float2(0 * dx, -1 * dx), float2(1 * dx, -1 * dx), float2(2 * dx, -1 * dx),
		float2(-2 * dx, 0 * dx), float2(-1 * dx, 0 * dx), float2(0 * dx, 0 * dx), float2(1 * dx, 0 * dx), float2(2 * dx, 0 * dx),
		float2(-2 * dx, 1 * dx), float2(-1 * dx, 1 * dx), float2(0 * dx, 1 * dx), float2(1 * dx, 1 * dx), float2(2 * dx, 1 * dx),
		float2(-2 * dx, 2 * dx), float2(-1 * dx, 2 * dx), float2(0 * dx, 2 * dx), float2(1 * dx, 2 * dx), float2(2 * dx, 2 * dx),
    };

	[unroll]
    for (int i = 0; i < 25; ++i)
    {
        offsets[i] = offsets[i] * float2(softness, softness);
        percentLit += shadowMap.SampleCmpLevelZero(samShadow,
			shadowPosH.xy + offsets[i], depth).r;
    }

    return percentLit /= 25.0f;

}

float CalcShadowFactor_PCF7x7(SamplerComparisonState samShadow,
	Texture2D shadowMap,
	float3 shadowPosH, int smSize, float softness)
{
    if (shadowPosH.z > 1.0f)
        return 1.0;
   
    float depth = shadowPosH.z;

    const float dx = 1.0f / smSize;

    float percentLit = 0.0f;

	[unroll]
    for (int i = -3; i <= 3; ++i)
    {
        for (int j = -3; j <= 3; ++j)
        {
            percentLit += shadowMap.SampleCmpLevelZero(samShadow,
				shadowPosH.xy + float2(i * dx * softness, j * dx * softness), depth).r;
        }

    }

    return percentLit /= 49;

}






static const float2 PoissonSamples[64] =
{
    float2(-0.5119625f, -0.4827938f),
	float2(-0.2171264f, -0.4768726f),
	float2(-0.7552931f, -0.2426507f),
	float2(-0.7136765f, -0.4496614f),
	float2(-0.5938849f, -0.6895654f),
	float2(-0.3148003f, -0.7047654f),
	float2(-0.42215f, -0.2024607f),
	float2(-0.9466816f, -0.2014508f),
	float2(-0.8409063f, -0.03465778f),
	float2(-0.6517572f, -0.07476326f),
	float2(-0.1041822f, -0.02521214f),
	float2(-0.3042712f, -0.02195431f),
	float2(-0.5082307f, 0.1079806f),
	float2(-0.08429877f, -0.2316298f),
	float2(-0.9879128f, 0.1113683f),
	float2(-0.3859636f, 0.3363545f),
	float2(-0.1925334f, 0.1787288f),
	float2(0.003256182f, 0.138135f),
	float2(-0.8706837f, 0.3010679f),
	float2(-0.6982038f, 0.1904326f),
	float2(0.1975043f, 0.2221317f),
	float2(0.1507788f, 0.4204168f),
	float2(0.3514056f, 0.09865579f),
	float2(0.1558783f, -0.08460935f),
	float2(-0.0684978f, 0.4461993f),
	float2(0.3780522f, 0.3478679f),
	float2(0.3956799f, -0.1469177f),
	float2(0.5838975f, 0.1054943f),
	float2(0.6155105f, 0.3245716f),
	float2(0.3928624f, -0.4417621f),
	float2(0.1749884f, -0.4202175f),
	float2(0.6813727f, -0.2424808f),
	float2(-0.6707711f, 0.4912741f),
	float2(0.0005130528f, -0.8058334f),
	float2(0.02703013f, -0.6010728f),
	float2(-0.1658188f, -0.9695674f),
	float2(0.4060591f, -0.7100726f),
	float2(0.7713396f, -0.4713659f),
	float2(0.573212f, -0.51544f),
	float2(-0.3448896f, -0.9046497f),
	float2(0.1268544f, -0.9874692f),
	float2(0.7418533f, -0.6667366f),
	float2(0.3492522f, 0.5924662f),
	float2(0.5679897f, 0.5343465f),
	float2(0.5663417f, 0.7708698f),
	float2(0.7375497f, 0.6691415f),
	float2(0.2271994f, -0.6163502f),
	float2(0.2312844f, 0.8725659f),
	float2(0.4216993f, 0.9002838f),
	float2(0.4262091f, -0.9013284f),
	float2(0.2001408f, -0.808381f),
	float2(0.149394f, 0.6650763f),
	float2(-0.09640376f, 0.9843736f),
	float2(0.7682328f, -0.07273844f),
	float2(0.04146584f, 0.8313184f),
	float2(0.9705266f, -0.1143304f),
	float2(0.9670017f, 0.1293385f),
	float2(0.9015037f, -0.3306949f),
	float2(-0.5085648f, 0.7534177f),
	float2(0.9055501f, 0.3758393f),
	float2(0.7599946f, 0.1809109f),
	float2(-0.2483695f, 0.7942952f),
	float2(-0.4241052f, 0.5581087f),
	float2(-0.1020106f, 0.6724468f),
};

float CalcShadowFactor_Poisson(SamplerComparisonState samShadow,
	Texture2D shadowMap,
	float4 shadowPosH, int smSize, float softness)
{
    if (shadowPosH.z > 1.0f)
        return 1.0;

    float depth = shadowPosH.z;

    const float dx = 1.0f / smSize;

    float percentLit = 0.0f;

	[unroll]
    for (int i = 0; i < 64; ++i)
    {
		
        percentLit += shadowMap.SampleCmpLevelZero(samShadow,
			shadowPosH.xy + PoissonSamples[i] * float2(dx * softness, dx * softness), depth).r;


    }

    return percentLit /= 64;
}


///////////////////////////////////		VSM/ESM/EVSM		///////////////////////////////////
/*
float Linstep(float a, float b, float v)
{
    return saturate((v - a) / (b - a));
}

float ReduceLightBleeding(float pMax, float amount)
{
	// Remove the [0, amount] tail and linearly rescale (amount, 1].
    return Linstep(amount, 1.0f, pMax);
}

float Chebyshev(float2 moments, float depth)
{

    if (depth <= moments.x)
    {
        return 1.0;
    }

    float variance = moments.y - (moments.x * moments.x);

    float d = depth - moments.x; // attenuation
    float pMax = variance / (variance + d * d);

    pMax = max(pMax, 0.01);

    return pMax;
}

float CalcShadowFactorVSM(SamplerState linearSampler, Texture2D shadowMap,
	float4 shadowPosH, float lightBleedingReduction)
{
    shadowPosH.xyz /= shadowPosH.w;

    float fragDepth = shadowPosH.z;

    float lit = 0.0f;
    float2 moments = shadowMap.Sample(linearSampler, shadowPosH.xy).rg;

    float p = Chebyshev(moments, fragDepth);

    p = ReduceLightBleeding(p, lightBleedingReduction);
    lit = max(p, fragDepth <= moments.x);

    return lit;
}


float CalcShadowFactorESM(SamplerState linearSampler, Texture2D shadowMap,
	float4 shadowPosH, float exponent) //,int smSize, int fallback, float softness)
{

    shadowPosH.xyz /= shadowPosH.w;
    float fragDepth = shadowPosH.z; // + 0.007;
    fragDepth = 2 * fragDepth - 1;
    float moment = shadowMap.Sample(linearSampler, shadowPosH.xy).r; //exp(depth);
    float visibility = exp(-exponent * fragDepth) * moment;
    return clamp(visibility, 0, 1);
}


float CalcShadowFactorEVSM(SamplerState linearSampler, Texture2D shadowMap,
	float4 shadowPosH, float lightBleedReduction, float exponent)
{
	
    shadowPosH.xyz /= shadowPosH.w;
    float fragDepth = shadowPosH.z;

    float shadow = 0.0;
    float4 moments = shadowMap.Sample(linearSampler, shadowPosH.xy); // pos, pos^2, neg, neg^2

    fragDepth = 2 * fragDepth - 1;
    float pos = exp(exponent * fragDepth);
    float neg = -exp(-exponent * fragDepth);


    float posShadow = Chebyshev(moments.xy, pos);
    float negShadow = Chebyshev(moments.zw, neg);

    posShadow = ReduceLightBleeding(posShadow, lightBleedReduction);
    negShadow = ReduceLightBleeding(negShadow, lightBleedReduction);

    shadow = min(posShadow, negShadow);
    return shadow;
}


///////////////////////////////////		MOMENT SHADOW MAPS		///////////////////////////////////

float4 ConvertOptimizedMoments(in float4 optimizedMoments)
{
    optimizedMoments[0] -= 0.035955884801f;
    return mul(optimizedMoments, float4x4(0.2227744146f, 0.1549679261f, 0.1451988946f, 0.163127443f,
		0.0771972861f, 0.1394629426f, 0.2120202157f, 0.2591432266f,
		0.7926986636f, 0.7963415838f, 0.7258694464f, 0.6539092497f,
		0.0319417555f, -0.1722823173f, -0.2758014811f, -0.3376131734f));
}
//from github MJP shadows
float ComputeMSMHamburger(in float4 moments, in float fragmentDepth)
{
	// Bias input data to avoid artifacts
    float4 b = lerp(moments, float4(0.5f, 0.5f, 0.5f, 0.5f), 0.001);
    float3 z;
    z[0] = fragmentDepth;

	// Compute a Cholesky factorization of the Hankel matrix B storing only non-
	// trivial entries or related products
    float L32D22 = mad(-b[0], b[1], b[2]);
    float D22 = mad(-b[0], b[0], b[1]);
    float squaredDepthVariance = mad(-b[1], b[1], b[3]);
    float D33D22 = dot(float2(squaredDepthVariance, -L32D22), float2(D22, L32D22));
    float InvD22 = 1.0f / D22;
    float L32 = L32D22 * InvD22;

	// Obtain a scaled inverse image of bz = (1,z[0],z[0]*z[0])^T
    float3 c = float3(1.0f, z[0], z[0] * z[0]);

	// Forward substitution to solve L*c1=bz
    c[1] -= b.x;
    c[2] -= b.y + L32 * c[1];

	// Scaling to solve D*c2=c1
    c[1] *= InvD22;
    c[2] *= D22 / D33D22;

	// Backward substitution to solve L^T*c3=c2
    c[1] -= L32 * c[2];
    c[0] -= dot(c.yz, b.xy);

	// Solve the quadratic equation c[0]+c[1]*z+c[2]*z^2 to obtain solutions
	// z[1] and z[2]
    float p = c[1] / c[2];
    float q = c[0] / c[2];
    float D = (p * p * 0.25f) - q;
    float r = sqrt(D);
    z[1] = -p * 0.5f - r;
    z[2] = -p * 0.5f + r;

	// Compute the shadow intensity by summing the appropriate weights
    float4 switchVal = (z[2] < z[0]) ? float4(z[1], z[0], 1.0f, 1.0f) :
		((z[1] < z[0]) ? float4(z[0], z[1], 0.0f, 1.0f) :
			float4(0.0f, 0.0f, 0.0f, 0.0f));
    float quotient = (switchVal[0] * z[2] - b[0] * (switchVal[0] + z[2]) + b[1]) / ((z[2] - switchVal[1]) * (z[0] - z[1]));
    float shadowIntensity = switchVal[2] + switchVal[3] * quotient;
    return saturate(shadowIntensity);
}

float CalcShadowFactorMSM(SamplerState linearSampler, Texture2D shadowMap,
	float4 shadowPosH, float lightleakfix)
{
    shadowPosH.xyz /= shadowPosH.w;
    float fragDepth = shadowPosH.z;

    float4 moments = shadowMap.Sample(linearSampler, shadowPosH.xy);

    float4 b = ConvertOptimizedMoments(moments); // moments = moments - float4(0.5, 0.0, 0.5, 0.0);

    return 1.0f - clamp(ComputeMSMHamburger(b, fragDepth) / lightleakfix, 0, 1);
}


///////////////////////////////////		PERCENTAGE CLOSER SOFT SHADOWS		///////////////////////////////////
#define BLOCKER_SEARCH_SAMPLES 16
#define PCF_SAMPLES 32

void BlockerSearch(out float avgBlockerDepth,
	out float numBlockers,
	Texture2D tDepthMap,
	SamplerState pointSampler,
	float2 uv,
	float z,
	float2 searchRegionRadiusUV)
{
    float blockerSum = 0;
    numBlockers = 0;

    for (int i = 0; i < BLOCKER_SEARCH_SAMPLES; ++i)
    {
        float2 offset = PoissonSamples16[i] * searchRegionRadiusUV;
        float shadowMapDepth = tDepthMap.SampleLevel(pointSampler, uv + offset, 0);
		
        if (shadowMapDepth < z - 0.001)
        {
            blockerSum += shadowMapDepth;
            numBlockers++;
        }
    }
    avgBlockerDepth = blockerSum / numBlockers;
}

float PCF_Filter(Texture2D tDepthMap,
	SamplerComparisonState shadowSampler, float2 uv, float z, float2 filterRadiusUV)
{
    float sum = 0;
    for (int i = 0; i < PCF_SAMPLES; ++i)
    {
        float2 offset = PoissonSamples32[i] * filterRadiusUV;
        sum += tDepthMap.SampleCmpLevelZero(shadowSampler, uv + offset, z);
    }
    return sum / PCF_SAMPLES;
}

float CalcShadowFactorPCSS(SamplerState pointSampler, SamplerComparisonState shadowSampler, Texture2D shadowMap,
	float4 shadowCoords, float shadowMapSize, float zEye, float softness, float frustumWidth, float frustumHeight, float zNear, float zFar)
{
	

    shadowCoords.xyz /= shadowCoords.w;
	// ------------------------
	// STEP 1: blocker search
	// ------------------------
    float avgBlockerDepth = 0;
    float numBlockers = 0;
    float2 g_LightRadiusUV = float2(1.0f / frustumWidth, 1.0f / frustumHeight); // * softness;

	// Using similar triangles from the surface point to the area light
    float2 searchRegionRadiusUV = g_LightRadiusUV * (zEye - zNear) / zEye; //softness*(zEye-0.02)/(zEye)
    BlockerSearch(avgBlockerDepth, numBlockers, shadowMap, pointSampler, shadowCoords.xy, shadowCoords.z, searchRegionRadiusUV);

	// Early out if no blocker found
    if (numBlockers == 0)
        return 1.0;
	// ------------------------
	// STEP 2: penumbra size
	// ------------------------
    float avgBlockerDepthWorld = zNear + avgBlockerDepth * (zFar - zNear);
	//zEye, avgBlockerDepthWorld
    float2 penumbraRadiusUV = g_LightRadiusUV * (zEye - avgBlockerDepthWorld) / avgBlockerDepthWorld;
	//penumbraRadius = fragDepth - avgBlockerDepth
    float2 filterRadiusUV = softness * penumbraRadiusUV; // *zNear / zEye;
	
    filterRadiusUV = max(filterRadiusUV, 0.3 / shadowMapSize);
	// STEP 3: filtering
	// ------------------------
    return PCF_Filter(shadowMap, shadowSampler, shadowCoords.xy, shadowCoords.z, filterRadiusUV);
}*/