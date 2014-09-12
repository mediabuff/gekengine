#pragma once

#include "GEKContext.h"
#include "GEKSystem.h"
#include "GEKAPI.h"
#include <concurrent_vector.h>

class CGEKComponentSystemModel : public CGEKUnknown
                               , public IGEKComponentSystem
                               , public IGEKSceneObserver
                               , public IGEKRenderObserver
{
public:
    struct MATERIAL
    {
        UINT32 m_nFirstVertex;
        UINT32 m_nFirstIndex;
        UINT32 m_nNumIndices;
    };

    struct MODEL
    {
        aabb m_nAABB;
        CComPtr<IGEKVideoBuffer> m_spPositionBuffer;
        CComPtr<IGEKVideoBuffer> m_spTexCoordBuffer;
        CComPtr<IGEKVideoBuffer> m_spBasisBuffer;
        CComPtr<IGEKVideoBuffer> m_spIndexBuffer;
        std::multimap<CComPtr<IUnknown>, MATERIAL> m_aMaterials;
    };

    struct INSTANCE
    {
        float4x4 m_nMatrix;
        float3 m_nScale;
        float m_nBuffer;

        INSTANCE(const float3 &nPosition, const quaternion &nRotation, const float3 &nScale)
            : m_nMatrix(nRotation, nPosition)
            , m_nScale(nScale)
            , m_nBuffer(0.0f)
        {
        }
    };

private:
    CComPtr<IGEKVideoBuffer> m_spInstanceBuffer;
    CComPtr<IUnknown> m_spVertexProgram;
    IGEKSceneManager *m_pSceneManager;
    IGEKRenderManager *m_pRenderManager;
    IGEKVideoSystem *m_pVideoSystem;
    IGEKMaterialManager *m_pMaterialManager;
    IGEKProgramManager *m_pProgramManager;

    concurrency::critical_section m_kCritical;
    std::unordered_map<CStringW, MODEL> m_aModels;
    std::unordered_map<MODEL *, std::vector<INSTANCE>> m_aVisible;

public:
    CGEKComponentSystemModel(void);
    virtual ~CGEKComponentSystemModel(void);
    DECLARE_UNKNOWN(CGEKComponentSystemModel);

    MODEL *GetModel(LPCWSTR pName, LPCWSTR pParams);

    // IGEKUnknown
    STDMETHOD(Initialize)                       (THIS);
    STDMETHOD_(void, Destroy)                   (THIS);

    // IGEKSceneObserver
    STDMETHOD(OnLoadEnd)                        (THIS_ HRESULT hRetVal);
    STDMETHOD_(void, OnFree)                    (THIS);

    // IGEKRenderObserver
    STDMETHOD_(void, OnPreRender)               (THIS);
    STDMETHOD_(void, OnCullScene)               (THIS);
    STDMETHOD_(void, OnDrawScene)               (THIS_ UINT32 nVertexAttributes);
    STDMETHOD_(void, OnPostRender)              (THIS);
};