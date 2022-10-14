
struct FrameCBuffer
{
	row_major matrix view;
	row_major matrix projection;
	row_major matrix viewProjection;
	row_major matrix inverseView;
	row_major matrix inverseProjection;
	row_major matrix inverseView_projection;
	row_major matrix prevViewProjection;
	float4 cameraPosition;
	float4 cameraForward;
	float  cameraNear;
	float  cameraFar;
	float2 screenResolution;
	float2 mouseNormalizedCoords;
};
ConstantBuffer<FrameCBuffer> FrameCB  : register(b0);

SamplerState LinearWrapSampler : register(s0);
SamplerState LinearClampSampler : register(s1);
SamplerState LinearBorderSampler : register(s2);

SamplerState PointWrapSampler : register(s3);
SamplerState PointClampSampler : register(s4);
SamplerState PointBorderSampler : register(s5);

SamplerComparisonState ShadowClampSampler : register(s6);
SamplerComparisonState ShadowWrapSampler : register(s7);
