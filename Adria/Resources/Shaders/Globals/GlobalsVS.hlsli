#include "../Util/CBuffers.hlsli"


ConstantBuffer<FrameCBuffer> frame_cbuf  : register(b0);
ConstantBuffer<ObjectCBuffer> object_cbuf : register(b1);
ConstantBuffer<ShadowCBuffer> shadow_cbuf : register(b3);