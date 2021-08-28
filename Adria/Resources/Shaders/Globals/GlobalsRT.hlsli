#include "../Util/CBuffers.hlsli"



ConstantBuffer<FrameCBuffer> frame_cbuf             : register(b0);
ConstantBuffer<LightCBuffer> light_cbuf             : register(b2);
ConstantBuffer<RayTracingCBuffer> ray_tracing_cbuf  : register(b10);