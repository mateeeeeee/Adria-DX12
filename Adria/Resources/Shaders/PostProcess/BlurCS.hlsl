#include "../Globals/GlobalsCS.hlsli"

Texture2D<float4>   InputMap        : register(t0);
RWTexture2D<float4> OutputMap       : register(u0);

const static uint size_x = 1024;
const static uint size_y = 1024;

// Declare the group shared memory to hold the loaded data 

#ifndef VERTICAL

groupshared float4 horizontalpoints[4 + size_x + 4];

[numthreads(size_x, 1, 1)]
void cs_main(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID, uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
    float filter_gauss[9] =
    {
        compute_cbuf.gauss_coeff1, compute_cbuf.gauss_coeff2, compute_cbuf.gauss_coeff3, compute_cbuf.gauss_coeff4, compute_cbuf.gauss_coeff5, 
        compute_cbuf.gauss_coeff6, compute_cbuf.gauss_coeff7, compute_cbuf.gauss_coeff8, compute_cbuf.gauss_coeff9
    };
    
	// Load the current data from input texture
    float4 data = InputMap.Load(DispatchThreadID);

	// Stor the data into the GSM for the current thread
    horizontalpoints[GroupThreadID.x + 4] = data;

	// Load the data to the left of this line for the first few pixels to use.
    if (GroupIndex == 0)
    {
        
        horizontalpoints[0] = InputMap.Load(DispatchThreadID - int3(4, 0, 0));
        horizontalpoints[1] = InputMap.Load(DispatchThreadID - int3(3, 0, 0));
        horizontalpoints[2] = InputMap.Load(DispatchThreadID - int3(2, 0, 0));
        horizontalpoints[3] = InputMap.Load(DispatchThreadID - int3(1, 0, 0));
    }

	// Load the data to the right of this line for the last few pixels to use.
    if (GroupIndex == size_x - 1)
    {
        horizontalpoints[4 + size_x + 0] = InputMap.Load(DispatchThreadID + int3(1, 0, 0));
        horizontalpoints[4 + size_x + 1] = InputMap.Load(DispatchThreadID + int3(2, 0, 0));
        horizontalpoints[4 + size_x + 2] = InputMap.Load(DispatchThreadID + int3(3, 0, 0));
        horizontalpoints[4 + size_x + 3] = InputMap.Load(DispatchThreadID + int3(4, 0, 0));
    }
	
	// Synchronize all threads
    GroupMemoryBarrierWithGroupSync();

	// Offset the texture location to the first sample location.  Since there are
	// three more samples loaded than there are threads, we start without an offset
	// into the group shared memory.  This allows all seven samples to be sequentially
	// accessed in a simple way.
    int texturelocation = GroupThreadID.x;

	// Initialize the output value to zero, then loop through the 
	// filter samples, apply them to the image samples, and sum
	// the results.
    float4 Color = float4(0.0, 0.0, 0.0, 0.0);

    for (int x = 0; x < 9; x++)
        Color += horizontalpoints[texturelocation + x] * filter_gauss[x];

	// Write the output to the output resource
    OutputMap[DispatchThreadID.xy] = Color;
}

#else

// Declare the group shared memory to hold the loaded data, including three
// extra samples on the left and right of this line.
groupshared float4 verticalpoints[4 + size_y + 4];

// For the vertical pass, use only a single column of threads
[numthreads(1, size_y, 1)]

void cs_main(uint3 GroupID : SV_GroupID, uint3 DispatchThreadID : SV_DispatchThreadID, uint3 GroupThreadID : SV_GroupThreadID, uint GroupIndex : SV_GroupIndex)
{
    float filter_gauss[9] =
    {
        compute_cbuf.gauss_coeff1, compute_cbuf.gauss_coeff2, compute_cbuf.gauss_coeff3, compute_cbuf.gauss_coeff4, compute_cbuf.gauss_coeff5, 
        compute_cbuf.gauss_coeff6, compute_cbuf.gauss_coeff7, compute_cbuf.gauss_coeff8, compute_cbuf.gauss_coeff9
    };
	// Load the current data from input texture
    float4 data = InputMap.Load(DispatchThreadID);

	// Stor the data into the GSM for the current thread
    verticalpoints[GroupThreadID.y + 4] = data;

	// Load the data to the top of this line for the first few pixels to use.
    if (GroupIndex == 0)
    {
        verticalpoints[0] = InputMap.Load(DispatchThreadID - int3(0, 4, 0));
        verticalpoints[1] = InputMap.Load(DispatchThreadID - int3(0, 3, 0));
        verticalpoints[2] = InputMap.Load(DispatchThreadID - int3(0, 2, 0));
        verticalpoints[3] = InputMap.Load(DispatchThreadID - int3(0, 1, 0));
    }

	// Load the data to the bottom of this line for the last few pixels to use.
    if (GroupIndex == size_y - 1)
    {
        verticalpoints[4 + size_y + 0] = InputMap.Load(DispatchThreadID + int3(0, 1, 0));
        verticalpoints[4 + size_y + 1] = InputMap.Load(DispatchThreadID + int3(0, 2, 0));
        verticalpoints[4 + size_y + 2] = InputMap.Load(DispatchThreadID + int3(0, 3, 0));
        verticalpoints[4 + size_y + 3] = InputMap.Load(DispatchThreadID + int3(0, 4, 0));
    }

	// Synchronize all threads
    GroupMemoryBarrierWithGroupSync();

	// Offset the texture location to the first sample location.  Since there are
	// three more samples loaded than there are threads, we start without an offset
	// into the group shared memory.  This allows all seven samples to be sequentially
	// accessed in a simple way.
    int texturelocation = GroupThreadID.y;

	// Initialize the output value to zero, then loop through the 
	// filter samples, apply them to the image samples, and sum
	// the results.
    float4 Color = float4(0.0, 0.0, 0.0, 0.0);

    for (int y = 0; y < 9; y++)
        Color += verticalpoints[texturelocation + y] * filter_gauss[y];

	// Write the output to the output resource
    OutputMap[DispatchThreadID.xy] = Color;
}

#endif