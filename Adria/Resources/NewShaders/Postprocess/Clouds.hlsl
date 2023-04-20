#include "CloudsUtil.hlsli"
#include "../Atmosphere.hlsli"
#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

#define BLOCK_SIZE 16
#define CONE_STEP 0.1666666

static const float4 StratusGradient = float4(0.0f, 0.1f, 0.2f, 0.3f);
static const float4 StratocumulusGradient = float4(0.02f, 0.2f, 0.48f, 0.625f);
static const float4 CumulusGradient = float4(0.0f, 0.1625f, 0.88f, 0.98f);
static const float  PlanetRadius = PLANET_RADIUS;
static const float3 PlanetCenter = PLANET_CENTER;

struct CloudsConstants
{
    float cloudsBottomHeight;
    float cloudsTopHeight;
    float crispiness;
    float curliness;
    float coverage;
    float cloudType;
    float absorption;
    float densityFactor;
};
ConstantBuffer<CloudsConstants> PassCB : register(b1);

struct CloudTextureIndices
{
    uint weatherIdx;
    uint cloudIdx;
    uint worleyIdx;
    uint depthIdx;
    uint outputIdx;
};
ConstantBuffer<CloudTextureIndices> TexturesCB : register(b2);


float3 ToClipSpaceCoord(float2 tex)
{
    float2 ray;
    ray.x = 2.0 * tex.x - 1.0;
    ray.y = 1.0 - tex.y * 2.0;

    return float3(ray, 1.0);
}
float GetHeightFraction(float3 inPos)
{
    float innerRadius = PlanetRadius + PassCB.cloudsBottomHeight;
    float outerRadius = innerRadius + PassCB.cloudsTopHeight;
    return (length(inPos - PlanetCenter) - innerRadius) / (outerRadius - innerRadius);
}
float2 GetUVProjection(float3 p)
{
    float innerRadius = PlanetRadius + PassCB.cloudsBottomHeight;
    return p.xz / innerRadius + 0.5f;
}
float GetDensityForCloud(float heightFraction, float cloudType)
{
    float stratusFactor = 1.0 - clamp(cloudType * 2.0, 0.0, 1.0);
    float stratoCumulusFactor = 1.0 - abs(cloudType - 0.5) * 2.0;
    float cumulusFactor = clamp(cloudType - 0.5, 0.0, 1.0) * 2.0;

    float4 baseGradient = stratusFactor * StratusGradient + stratoCumulusFactor * StratocumulusGradient + cumulusFactor * CumulusGradient;
    return smoothstep(baseGradient.x, baseGradient.y, heightFraction) - smoothstep(baseGradient.z, baseGradient.w, heightFraction);
}
float SampleCloudDensity(float3 p, bool useHighFreq, float lod)
{
    Texture3D cloudTx = ResourceDescriptorHeap[TexturesCB.cloudIdx];
    Texture3D worleyTx = ResourceDescriptorHeap[TexturesCB.worleyIdx];
    Texture2D weatherTx = ResourceDescriptorHeap[TexturesCB.weatherIdx];
    
    float heightFraction = GetHeightFraction(p);
    float3 scroll = FrameCB.windParams.xyz * (heightFraction * 750.0f + FrameCB.totalTime * FrameCB.windParams.w);
    
    float2 UV = GetUVProjection(p);
    float2 dynamicUV = GetUVProjection(p + scroll);

    if (heightFraction <= 0.0f || heightFraction >= 1.0f) return 0.0f;

    float4 lowFreqNoise = cloudTx.SampleLevel(LinearWrapSampler, float3(UV * PassCB.crispiness, heightFraction), lod);
    float lowFreqFBM = dot(lowFreqNoise.gba, float3(0.625, 0.25, 0.125));
    float cloudSample = Remap(lowFreqNoise.r, -(1.0f - lowFreqFBM), 1.0f, 0.0f, 1.0f);
	 
    float density = GetDensityForCloud(heightFraction, PassCB.cloudType);
    cloudSample *= density / max(heightFraction, 0.001f);

    float3 weatherNoise = weatherTx.Sample(LinearWrapSampler, dynamicUV).rgb;
    float cloudWeatherCoverage = weatherNoise.r * PassCB.coverage;
    float cloudSampleWithCoverage = Remap(cloudSample, cloudWeatherCoverage, 1.0f, 0.0f, 1.0f);
    cloudSampleWithCoverage *= cloudWeatherCoverage;

    // high frequency sample
    if (useHighFreq)
    {
        float3 highFreqNoise = worleyTx.SampleLevel(LinearWrapSampler, float3(dynamicUV * PassCB.crispiness, heightFraction) * PassCB.curliness, lod).rgb;
        float highFreqFBM = dot(highFreqNoise.rgb, float3(0.625, 0.25, 0.125));
        float highFreqNoiseModifier = lerp(highFreqFBM, 1.0f - highFreqFBM, clamp(heightFraction * 10.0f, 0.0f, 1.0f));
        cloudSampleWithCoverage = cloudSampleWithCoverage - highFreqNoiseModifier * (1.0 - cloudSampleWithCoverage);
        cloudSampleWithCoverage = Remap(cloudSampleWithCoverage * 2.0, highFreqNoiseModifier * 0.2, 1.0f, 0.0f, 1.0f);
    }

    return clamp(cloudSampleWithCoverage, 0.0f, 1.0f);
}
float RaymarchToLight(float3 origin, float stepSize, float3 lightDir, float originalDensity, float lightDotEye)
{
    float3 startPos = origin;
    
    float deltaStep = stepSize * 6.0f;
    float3 rayStep = lightDir * deltaStep;
    const float coneStep = 1.0f / 6.0f;
    float coneRadius = 1.0f;
    float coneDensity = 0.0;
    
    float density = 0.0;
    const float densityThreshold = 0.3f;
    
    float invDepth = 1.0 / deltaStep;
    float sigmaDeltaStep = -deltaStep * PassCB.absorption;
    float3 pos;

    float finalTransmittance = 1.0;

    [unroll(16)]
    for (int i = 0; i < 16; i++)
    {
        pos = startPos + coneRadius * NoiseKernelConeSampling[i] * float(i);

        float heightFraction = GetHeightFraction(pos);
        if (heightFraction >= 0)
        {
            float cloudDensity = SampleCloudDensity(pos, density < densityThreshold, i / 16.0f);
            
            if (cloudDensity > 0.0)
            {
                density += cloudDensity;
                float transmittance = exp(sigmaDeltaStep * density);
                coneDensity += (cloudDensity * transmittance);
            }
        }
        startPos += rayStep;
        coneRadius += coneStep;
    }
    return LightEnergy(coneDensity, lightDotEye);
}
float4 RaymarchToCloud(uint2 globalCoord, float3 startPos, float3 endPos, float3 skyColor, out float4 cloudPos)
{
    const float minTransmittance = 0.1f;
    const int steps = 32;
    float4 finalColor = float4(0.0, 0.0, 0.0, 0.0);
    
    float3 path = endPos - startPos;
    float len = length(path);
	
    float deltaStep = len / (float) steps;
    float3 dir = path / len;
    dir *= deltaStep;
    
    int a = int(globalCoord.x) % 4;
    int b = int(globalCoord.y) % 4;
    startPos += dir * BayerFilter[a * 4 + b];

    float3 pos = startPos;
    float density = 0.0f;
    float LdotV = dot(normalize(FrameCB.sunDirection.xyz), normalize(dir));

    float finalTransmittance = 1.0f;
    float sigmaDeltaStep = -deltaStep * PassCB.densityFactor;
    bool entered = false;

    [unroll(steps)]
    for (int i = 0; i < steps; ++i)
    {
        float densitySample = SampleCloudDensity(pos, true, i / 16.0f);
        if (densitySample > 0.0f)
        {
            if (!entered)
            {
                cloudPos = float4(pos, 1.0);
                entered = true;
            }
            density += densitySample;
            float lightEnergy = RaymarchToLight(pos, deltaStep * 0.1f, FrameCB.sunDirection.xyz, densitySample, LdotV); 

            float height = GetHeightFraction(pos);
            float4 src = float4(FrameCB.sunColor.rgb * lightEnergy, densitySample);
            src.rgb *= src.a;
            finalColor = (1.0 - finalColor.a) * src + finalColor;

            if (finalColor.a >= 0.95)  break;
        }
        if (finalTransmittance <= minTransmittance) break;
        pos += dir;
    }
    return finalColor;
}
bool IntersectSphere(float3 o, float3 d, out float3 minT, out float3 maxT)
{
    float innerRadius = PlanetRadius + PassCB.cloudsBottomHeight;
    float outerRadius = innerRadius  + PassCB.cloudsTopHeight;
	
    float3 sphereToOrigin = (o - PlanetCenter);
    float b = dot(d, sphereToOrigin);
    float c = dot(sphereToOrigin, sphereToOrigin);
    float sqrtOpInner = b * b - (c - innerRadius * innerRadius);

    float maxSInner;
    if (sqrtOpInner < 0.0) return false;

    float deInner = sqrt(sqrtOpInner);
    float solAInner = -b - deInner;
    float solBInner = -b + deInner;
    maxSInner = max(solAInner, solBInner);
    if (maxSInner < 0.0) return false;

    maxSInner = maxSInner < 0.0 ? 0.0 : maxSInner;
    float sqrtOpOuter = b * b - (c - outerRadius * outerRadius);
    if (sqrtOpOuter < 0.0) return false;

    float deOuter = sqrt(sqrtOpOuter);
    float solAOuter = -b - deOuter;
    float solBOuter = -b + deOuter;
    float maxSOuter = max(solAOuter, solBOuter);
    if (maxSOuter < 0.0) return false;

    maxSOuter = maxSOuter < 0.0 ? 0.0 : maxSOuter;
    float minSol = min(maxSInner, maxSOuter);
    
    if (minSol > PlanetRadius * 0.3f) return false;
    float maxSol = max(maxSInner, maxSOuter);

    minT = o + d * minSol;
    maxT = o + d * maxSol;
    return true;
}

struct CS_INPUT
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void Clouds(CS_INPUT input)
{
    Texture2D depthTx = ResourceDescriptorHeap[TexturesCB.depthIdx];
    RWTexture2D<float4> outputTx = ResourceDescriptorHeap[TexturesCB.outputIdx];
   
    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);
   
    float4 cloudsColor = float4(0.0, 0.0, 0.0, 0.0f);
    float depth = depthTx.SampleLevel(PointWrapSampler, uv, 0).r;
    if (depth < 0.99999f)
    {
        outputTx[input.DispatchThreadId.xy] = 0.0f;
        return;
    }
    
    float4 rayClipSpace = float4(ToClipSpaceCoord(uv), 1.0);
    float4 rayView = mul(rayClipSpace, FrameCB.inverseProjection);
    rayView = float4(rayView.xy, 1.0, 0.0);

    float3 worldDir = mul(rayView, FrameCB.inverseView).xyz;
    worldDir = normalize(worldDir);
    
    float3 startPos, endPos;
    bool intersect = IntersectSphere(FrameCB.cameraPosition.xyz, worldDir, startPos, endPos);
    
    if (intersect)
    {
        float4 cloudDistance;
        cloudsColor = RaymarchToCloud(input.DispatchThreadId.xy, startPos, endPos, float3(0, 0, 0), cloudDistance);
        outputTx[input.DispatchThreadId.xy] = cloudsColor;
    }
    else
    {
        outputTx[input.DispatchThreadId.xy] = 0.0f;
    }
}