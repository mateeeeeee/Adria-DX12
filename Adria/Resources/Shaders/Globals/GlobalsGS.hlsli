#include "../Util/CBuffers.hlsli"

ConstantBuffer<LightCBuffer> light_cbuf             : register(b2);
ConstantBuffer<PostprocessCBuffer> postprocess_cbuf : register(b5);
