
static float Luminance(float3 color)
{
	return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}
