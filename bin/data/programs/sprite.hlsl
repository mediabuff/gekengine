struct INSTANCE
{
    float3      m_nPosition;
    float       m_nHalfSize;
    float4      m_nColor;
};
            
StructuredBuffer<INSTANCE> gs_aInstances    : register(t0);

struct SOURCEVERTEX
{
	float2 position                         : POSITION;
	float2 texcoord                         : TEXCOORD0;
    uint instance                           : SV_InstanceId;
};

WORLDVERTEX GetWorldVertex(in SOURCEVERTEX kSource)
{
    INSTANCE kInstance = gs_aInstances[kSource.instance];
	float3 kXOffSet = mul(float3(kInstance.m_nHalfSize, 0.0, 0.0f), (float3x3)gs_nViewMatrix);
	float3 kYOffSet = mul(float3(0.0,-kInstance.m_nHalfSize, 0.0f), (float3x3)gs_nViewMatrix);

	WORLDVERTEX kVertex;
	kVertex.position      = float4(kInstance.m_nPosition, 1.0f);
	kVertex.position.xyz += (kXOffSet * kSource.position.x);
	kVertex.position.xyz += (kYOffSet * kSource.position.y);
	kVertex.texcoord      = kSource.texcoord;
    kVertex.normal        = mul(float3(0,0,-1), (float3x3)gs_nViewMatrix);
    kVertex.color         = kInstance.m_nColor;
	return kVertex;
}
