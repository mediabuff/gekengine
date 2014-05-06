cbuffer ENGINEBUFFER                    : register(b0)
{
    float4x4 gs_nViewMatrix;
    float4x4 gs_nProjectionMatrix;
    float4x4 gs_nTransformMatrix;
    float3   gs_nCameraPosition;
    float    gs_nCameraViewDistance;
    float2   gs_nCameraView;
    float2   gs_nCameraSize;
};

cbuffer TILESBUFFER                     : register(b1)
{
    uint2   gs_nNumTiles;
    uint2   gs_nTilePadding;
}

struct LIGHT
{
    float3 m_nPosition;
    float  m_nRange;
    float3 m_nColor;
    float  m_nInvRange;
};

StructuredBuffer<LIGHT> gs_aLights      : register(t3);

static const uint gs_nThreadGroupSize = gs_nLightTileSize * gs_nLightTileSize;

Texture2D<float> Depth                  : register(t1);

RWBuffer<uint> g_pTileOutput            : register(u0);

groupshared uint g_nTileMinDepth;
groupshared uint g_nTileMaxDepth;
groupshared uint g_aTileLightList[gs_nMaxLights];
groupshared uint g_nNumTileLights;

float LinearDepth(in float zw)
{
    return gs_nProjectionMatrix._43 / (zw - gs_nProjectionMatrix._33);
}

[numthreads(gs_nLightTileSize, gs_nLightTileSize, 1)]
void MainComputeProgram(uint3 nGroupID : SV_GroupID, uint3 nGroupThreadID : SV_GroupThreadID)
{
    uint2 nPixelCoord = nGroupID.xy * uint2(gs_nLightTileSize, gs_nLightTileSize) + nGroupThreadID.xy;
    const uint nGroupThreadIndex = nGroupThreadID.y * gs_nLightTileSize + nGroupThreadID.x;

    float nMinSampleDepth = gs_nCameraViewDistance;
    float nMaxSampleDepth = 0.0f;

    float zw = Depth[nPixelCoord];
    float linearZ = LinearDepth(zw);
    nMinSampleDepth = min(nMinSampleDepth, linearZ);
    nMaxSampleDepth = max(nMaxSampleDepth, linearZ);

    if (nGroupThreadIndex == 0)
    {
        g_nNumTileLights = 0;
        g_nTileMinDepth = 0x7F7FFFFF;
        g_nTileMaxDepth = 0;
    }

    GroupMemoryBarrierWithGroupSync();
    if (nMaxSampleDepth >= nMinSampleDepth)
    {
        InterlockedMin(g_nTileMinDepth, asuint(nMinSampleDepth));
        InterlockedMax(g_nTileMaxDepth, asuint(nMaxSampleDepth));
    }

    GroupMemoryBarrierWithGroupSync();

    float nMinTileDepth = asfloat(g_nTileMinDepth);
    float nMaxTileDepth = asfloat(g_nTileMaxDepth);

    float2 nTileScale = float2(gs_nCameraSize.xy) * rcp(2.0f * float2(gs_nLightTileSize, gs_nLightTileSize));
    float2 nTileBias = nTileScale - float2(nGroupID.xy);
    float4 c1 = float4(gs_nProjectionMatrix._11 * nTileScale.x, 0.0f, nTileBias.x, 0.0f);
    float4 c2 = float4(0.0f, -gs_nProjectionMatrix._22 * nTileScale.y, nTileBias.y, 0.0f);
    float4 c4 = float4(0.0f, 0.0f, 1.0f, 0.0f);
    float4 aFrustumPlanes[6] = 
    {
        // Sides
        c4 - c1,
        c4 + c1,
        c4 - c2,
        c4 + c2,

        // Near/far
        float4(0.0f, 0.0f, 1.0f, -nMinTileDepth),
        float4(0.0f, 0.0f, -1.0f, nMaxTileDepth),
    };

    [unroll]
    for (uint nIndex = 0; nIndex < 4; ++nIndex)
    {
        aFrustumPlanes[nIndex] *= rcp(length(aFrustumPlanes[nIndex].xyz));
    }

    [unroll]
    for (uint nLightIndex = nGroupThreadIndex; nLightIndex < gs_nMaxLights; nLightIndex += gs_nThreadGroupSize)
    {
        float3 nLightPosition = gs_aLights[nLightIndex].m_nPosition;
        float nLightRange = gs_aLights[nLightIndex].m_nRange;

        bool bIsLightInFrustum = true;

        [unroll]
        for (uint nIndex = 0; nIndex < 6; ++nIndex)
        {
            float d = dot(aFrustumPlanes[nIndex], float4(nLightPosition, 1.0f));
            bIsLightInFrustum = bIsLightInFrustum && (d >= -nLightRange);
        }

        [branch]
        if (bIsLightInFrustum)
        {
            uint nListIndex;
            InterlockedAdd(g_nNumTileLights, 1, nListIndex);
            g_aTileLightList[nListIndex] = nLightIndex;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    [branch]
    if (nGroupThreadIndex < gs_nMaxLights)
    {
        uint nTileIndex = nGroupID.y * gs_nNumTiles.x + nGroupID.x;
        uint nBufferIndex = nTileIndex * gs_nMaxLights + nGroupThreadIndex;
        uint nLightIndex = nGroupThreadIndex < g_nNumTileLights ? g_aTileLightList[nGroupThreadIndex] : gs_nMaxLights;
        g_pTileOutput[nBufferIndex] = nLightIndex;
    }
}