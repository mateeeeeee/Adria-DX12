struct VSToPS
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEX;

};

VSToPS FullscreenTriangle(uint vertexId : SV_VERTEXID)
{
	VSToPS output = (VSToPS)0;
	uint2 v = uint2(vertexId & 1, vertexId >> 1);
	output.Pos = float4(4.0f * float2(v) - 1.0f, 0, 1);
	output.Tex.x = v.x * 2.0f;
	output.Tex.y = 1.0f - v.y * 2.0f;
	return output;
}