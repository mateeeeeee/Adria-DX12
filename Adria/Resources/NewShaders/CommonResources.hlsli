
struct FrameCBuffer
{
	float4 global_ambient;
	row_major matrix view;
	row_major matrix projection;
	row_major matrix viewprojection;
	row_major matrix inverse_view;
	row_major matrix inverse_projection;
	row_major matrix inverse_view_projection;
	row_major matrix prev_view_projection;
	float4 camera_position;
	float4 camera_forward;
	float  camera_near;
	float  camera_far;
	float2 screen_resolution;
	float2 mouse_normalized_coords;
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
