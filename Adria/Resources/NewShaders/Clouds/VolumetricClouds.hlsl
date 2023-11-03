#include "../CommonResources.hlsli"
#define BLOCK_SIZE 16

struct CloudsConstants
{
	uint      typeIdx;
	uint      shapeIdx;
	uint      detailIdx;
	uint      outputIdx;

	uint      prevOutputIdx;
	float     cloudType;
	float 	  cloudMinHeight;
	float 	  cloudMaxHeight;

	float 	  shapeNoiseScale;
	float 	  detailNoiseScale;
	float 	  detailNoiseModifier;
	float     globalDensity;
	
	float 	  cloudCoverage;
	float3    cloudBaseColor;
	float3    cloudTopColor;
	int	      maxNumSteps;

	float3    planetCenter;
	float 	  planetRadius;

	float 	  lightStepLength;
	float 	  lightConeRadius;
	float 	  precipitation;
	float 	  ambientLightFactor;

	float 	  sunLightFactor;
	float 	  henyeyGreensteinGForward;
	float 	  henyeyGreensteinGBackward;
	uint      resolutionFactor;
};
ConstantBuffer<CloudsConstants> PassCB : register(b2);

static const float BayerFactor = 1.0f / 16.0f;
static const int BayerFilter[16] =
{
	0, 8, 2, 10,
	12, 4, 14, 6,
	3, 11, 1, 9,
	15, 7, 13, 5
};

struct Ray
{
	float3 origin;
	float3 direction;
};

float3 ToClipSpaceCoord(float2 uv)
{
	float2 ray;
	ray.x = 2.0 * uv.x - 1.0;
	ray.y = 1.0 - uv.y * 2.0;
	return float3(ray, 1.0);
}

bool IntersectSphere(Ray ray, out float3 minT, out float3 maxT)
{
	float innerRadius = PassCB.planetRadius + PassCB.cloudMinHeight;
	float outerRadius = innerRadius + PassCB.cloudMaxHeight;

	float3 sphereToOrigin = (ray.origin - PassCB.planetCenter);
	float b = dot(ray.direction, sphereToOrigin);
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

	if (minSol > PassCB.planetRadius * 0.3f) return false;
	float maxSol = max(maxSInner, maxSOuter);

	minT = ray.origin + ray.direction * minSol;
	maxT = ray.origin + ray.direction * maxSol;
	return true;
}
float3 RaySphereIntersection(Ray ray, float3 sphereCenter, in float sphereRadius, out bool hit)
{
	hit = true;
	float3 l = ray.origin - sphereCenter;
	float a = 1.0;
	float b = 2.0 * dot(ray.direction, l);
	float c = dot(l, l) - pow(sphereRadius, 2);
	float D = pow(b, 2) - 4.0 * a * c;

	if (D < 0.0)
	{
		hit = false;
		return ray.origin;
	}
	else if (abs(D) - 0.00005 <= 0.0) return ray.origin + ray.direction * (-0.5 * b / a);
	else
	{
		float q = 0.0;
		if (b > 0.0) q = -0.5 * (b + sqrt(D));
		else q = -0.5 * (b - sqrt(D));

		float h1 = q / a;
		float h2 = c / q;
		float2 t = float2(min(h1, h2), max(h1, h2));

		if (t.x < 0.0)
		{
			t.x = t.y;
			if (t.x < 0.0) return ray.origin;
		}
		return ray.origin + t.x * ray.direction;
	}
}
float Remap(float originalValue, float originalMin, float originalMax, float newMin, float newMax)
{
	return newMin + (((originalValue - originalMin) / (originalMax - originalMin)) * (newMax - newMin));
}
float HeightFractionForPoint(float3 position)
{
	return clamp((distance(position, PassCB.planetCenter) - (PassCB.planetRadius + PassCB.cloudMinHeight)) / (PassCB.cloudMaxHeight - PassCB.cloudMinHeight), 0.0f, 1.0f);
}
float BeerLambertLaw(float density)
{
	return exp(-density * PassCB.precipitation);
}
float BeerLaw(float density)
{
	float d = -density * PassCB.precipitation;
	return max(exp(d), exp(d * 0.5f) * 0.7f);
}
float HenyeyGreensteinPhase(float cosAngle, float g)
{
	float g2 = g * g;
	return ((1.0f - g2) / pow(1.0f + g2 - 2.0f * g * cosAngle, 1.5f)) / 4.0f * 3.1415f;
}
float PowderEffect(float density, float cosAngle)
{
	float powder = 1.0f - exp(-density * 2.0f);
	return lerp(1.0f, powder, clamp((-cosAngle * 0.5f) + 0.5f, 0.0f, 1.0f));
}
float CalculateLightEnergy(float density, float cosAngle, float powderDensity)
{
	float beerPowder = 2.0f * BeerLaw(density) * PowderEffect(powderDensity, cosAngle);
	float HG = max(HenyeyGreensteinPhase(cosAngle, PassCB.henyeyGreensteinGForward), HenyeyGreensteinPhase(cosAngle, PassCB.henyeyGreensteinGBackward)) * 0.07f + 0.8f;
	return beerPowder * HG;
}

float SampleCloudDensity(float3 positionStatic, float heightFraction, float lod, bool useDetail)
{
	Texture3D shapeTx = ResourceDescriptorHeap[PassCB.shapeIdx];
	Texture3D detailTx = ResourceDescriptorHeap[PassCB.detailIdx];
	Texture2D typeTx = ResourceDescriptorHeap[PassCB.typeIdx];

	float3 position = positionStatic + FrameCB.windParams.xyz * heightFraction;
	position += (FrameCB.windParams.xyz + float3(0.0f, 0.1f, 0.0f)) * FrameCB.windParams.w * (FrameCB.totalTime + 256.0f);

	float4 lowFrequencyNoises = shapeTx.SampleLevel(LinearWrapSampler, position * PassCB.shapeNoiseScale, lod);
	float lowFreqFbm = (lowFrequencyNoises.g * 0.625f) + (lowFrequencyNoises.b * 0.25f) + (lowFrequencyNoises.a * 0.125f);
	float baseCloud = Remap(lowFrequencyNoises.r, (1.0f - lowFreqFbm), 1.0f, 0.0f, 1.0f);
	float cloudCoverage =  PassCB.cloudCoverage;
	float baseCloudWithCoverage = Remap(baseCloud, 1.0f - cloudCoverage, 1.0f, 0.0f, 1.0f);

	baseCloudWithCoverage *= cloudCoverage;
	float finalCloud = baseCloudWithCoverage;

	float verticalDensity = typeTx.SampleLevel(LinearWrapSampler, float2(PassCB.cloudType, heightFraction), 0).x;
	baseCloudWithCoverage *= verticalDensity;

	if (baseCloudWithCoverage <= 0.0f) return 0.0f;

	if (useDetail)
	{
		float3 highFrequencyNoises = detailTx.SampleLevel(LinearWrapSampler, position * PassCB.detailNoiseScale, lod).xyz; 
		float highFreqFbm = (highFrequencyNoises.r * 0.625f) + (highFrequencyNoises.g * 0.25f) + (highFrequencyNoises.b * 0.125f);
		float highFreqNoiseModifier = lerp(1.0f - highFreqFbm, highFreqFbm, clamp(heightFraction * 10.0f, 0.0f, 1.0f));
		finalCloud = Remap(baseCloudWithCoverage, highFreqNoiseModifier * PassCB.detailNoiseModifier, 1.0f, 0.0f, 1.0f);
	}

	return saturate(finalCloud * PassCB.globalDensity);
}
float SampleCloudDensityAlongCone(float3 position, float3 lightDir)
{
	const float3 noiseKernel[6] =
	{
		{ -0.6, -0.8, -0.2 },
		{ 1.0, -0.3, 0.0 },
		{ -0.7, 0.0, 0.7 },
		{ -0.2, 0.6, -0.8 },
		{ 0.4, 0.3, 0.9 },
		{ -0.2, 0.6, -0.8 }
	};

	float densityAlongCone = 0.0f;
	const int NUM_CONE_SAMPLES = 6;
	for (int i = 0; i < NUM_CONE_SAMPLES; i++)
	{
		position += lightDir * PassCB.lightStepLength;
		float3 random_offset = noiseKernel[i] * PassCB.lightStepLength * PassCB.lightConeRadius * (float(i + 1));
		float3 p = position + random_offset;
		float heightFraction = HeightFractionForPoint(p); 
		bool useDetailNoise = i < 2;
		densityAlongCone += SampleCloudDensity(p, heightFraction, float(i) * 0.5f, useDetailNoise);
	}
	position += 32.0f * PassCB.lightStepLength * lightDir;
	float heightFraction = HeightFractionForPoint(position);
	densityAlongCone += SampleCloudDensity(position, heightFraction, 2.0f, false) * 3.0f;

	return densityAlongCone;
}

float4 RayMarch(float3 rayOrigin, float3 rayDirection, float cosAngle, float stepSize, float numSteps)
{
	float3  position = rayOrigin;
	float   stepIncrement = 1.0f;
	float   accumTransmittance = 1.0f;
	float3  accumScattering = 0.0f;
	float   alpha = 0.0f;
	float3 sunColor = FrameCB.sunColor.rgb;
	float3 sunDirection = normalize(FrameCB.sunDirection.xyz);

	for (float i = 0.0f; i < numSteps; i += stepIncrement)
	{
		float heightFraction = HeightFractionForPoint(position);
		float density = SampleCloudDensity(position, heightFraction, 0.0f, true);
		float stepTransmittance = BeerLambertLaw(density * stepSize);

		accumTransmittance *= stepTransmittance;

		if (density > 0.0f)
		{
			alpha += (1.0f - stepTransmittance) * (1.0f - alpha);

			float coneDensity = SampleCloudDensityAlongCone(position, sunDirection);
			float3 inScatteredLight = CalculateLightEnergy(coneDensity * stepSize, cosAngle, density * stepSize) * sunColor * PassCB.sunLightFactor * alpha;
			float3 ambientLight = lerp(PassCB.cloudBaseColor, PassCB.cloudTopColor, heightFraction) * PassCB.ambientLightFactor;
			accumScattering += (ambientLight + inScatteredLight) * accumTransmittance * density;

			if (alpha > 0.99f || accumTransmittance < 0.01f) break;
		}
		position += rayDirection * stepSize * stepIncrement;
	}

	return float4(accumScattering, alpha);
}


struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CloudsCS(CSInput input)
{
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	uint3 threadId = input.DispatchThreadId;
	uint2 resolution = uint2(FrameCB.renderResolution) >> PassCB.resolutionFactor;
	float2 uv = ((float2) threadId.xy + 0.5f) * 1.0f / resolution;

#if REPROJECTION
	float4 prevPos = float4(ToClipSpaceCoord(uv), 1.0);
	prevPos = mul(prevPos, FrameCB.inverseProjection);
	prevPos = prevPos / prevPos.w;
	prevPos.xyz = mul(prevPos.xyz, (float3x3) FrameCB.inverseView);
	prevPos.xyz = mul(prevPos.xyz, (float3x3) FrameCB.prevView);
	float4 reproj = mul(prevPos, FrameCB.prevProjection);
	reproj /= reproj.w;
	float2 prevUv = reproj.xy * 0.5 + 0.5;
	prevUv.y = 1.0f - prevUv.y;

	Texture2D<float4> prevOutputTx = ResourceDescriptorHeap[PassCB.prevOutputIdx];
	float4 prevColor = prevOutputTx.Sample(LinearClampSampler, prevUv);

	uint index = FrameCB.frameCount % 16;
	bool shouldUpdate = (((threadId.x + 4 * threadId.y) % 16) == BayerFilter[index]);

	if (!shouldUpdate && all(prevUv <= 1.0f) && all(prevUv >= 0.0f))
	{
		outputTx[threadId.xy] = prevColor;
		return;
	}
#endif

	float4 rayClipSpace = float4(ToClipSpaceCoord(uv), 1.0);
	float4 rayView = mul(rayClipSpace, FrameCB.inverseProjection);
	rayView = float4(rayView.xy, 1.0, 0.0);
	float3 worldDir = mul(rayView, FrameCB.inverseView).xyz;

	Ray ray;
	ray.origin = FrameCB.cameraPosition.xyz;
	ray.direction = normalize(worldDir);

	float3 rayStart, rayEnd;
	bool intersect = IntersectSphere(ray, rayStart, rayEnd);

	if (!intersect)
	{
		outputTx[threadId.xy] = 0.0f;
		return;
	}

	int a = int(threadId.x) % 4;
	int b = int(threadId.y) % 4;

	const float maxSteps = PassCB.maxNumSteps;
	const float minSteps = (maxSteps * 0.5f) + BayerFilter[a * 4 + b] / 8.0f;
	float numSteps = lerp(maxSteps, minSteps, ray.direction.y);
	float stepSize = length(rayEnd - rayStart) / numSteps;

	rayStart += stepSize * ray.direction * BayerFactor * BayerFilter[a * 4 + b];

	float cosAngle = dot(ray.direction, normalize(FrameCB.sunDirection.xyz));
	float4 clouds = RayMarch(rayStart, ray.direction, cosAngle, stepSize, numSteps);

	outputTx[threadId.xy] = clouds;
}


//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
#define CLOUDS_DEPTH 0.99999f

struct VSToPS
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEX;
};
VSToPS CloudsCombineVS(uint vertexId : SV_VERTEXID)
{
	VSToPS output = (VSToPS)0;
	uint2 v = uint2(vertexId & 1, vertexId >> 1);
	output.Pos = float4(4.0f * float2(v) - 1.0f, CLOUDS_DEPTH, 1);
	output.Tex.x = v.x * 2.0f;
	output.Tex.y = 1.0f - v.y * 2.0f;
	return output;
}

struct CloudsCombineConstants
{
	uint inputIdx;
};
ConstantBuffer<CloudsCombineConstants> CombineCB : register(b1);

float4 CloudsCombinePS(VSToPS input) : SV_Target0
{
	Texture2D<float4> inputTx = ResourceDescriptorHeap[CombineCB.inputIdx];
	float4 color = inputTx.Sample(LinearWrapSampler, input.Tex);
	if (!any(color.xyz) || color.a < 0.03f) discard;
	return color;
}