//--------------------------------------------------------------------------------------
// File: ComputeRowsAvgColors.hlsl
//
// This file contains the Compute Shader to perform
// 
//--------------------------------------------------------------------------------------


struct Pixel {
    int color;
};

Texture2D<unorm float3> BufferIn : register(t0);
RWStructuredBuffer<Pixel> BufferOut : register(u0);
cbuffer ParamsStruct : register(b0)
{
    uint width;
    uint height;
    uint targetWidth;
    uint unused;
};


void writeResult(int x, int y, float3 colour)
{
    uint index = (x + y * targetWidth);

    int ired = (int)(clamp(colour.r, 0, 1) * 255);
    int igreen = (int)(clamp(colour.g, 0, 1) * 255) << 8;
    int iblue = (int)(clamp(colour.b, 0, 1) * 255) << 16;

    BufferOut[index].color = ired + igreen + iblue;
}

[numthreads(512, 2, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    int start, end;
    if (DTid.y == 0) {
        start = 0;
        end = height / 2;
    }
    else {
        start = height / 2;
        end = height;
    }


    float3 sum = 0;
    
    for (int y = start; y < end; y++) {
        sum += BufferIn[int2(DTid.x, y)];
    }

    writeResult(DTid.x, DTid.y & 1, sum / (height / 2));
}