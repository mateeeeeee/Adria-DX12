struct TextureChannelMappingConstants
{
	uint srcIdx;
	uint dstIdx;
};
ConstantBuffer<TextureChannelMappingConstants> PassCB : register(b1);


[numthreads(16, 16, 1)]
void TextureChannelMappingCS(uint3 dispatchThreadId : SV_DispatchThreadID)
{

}