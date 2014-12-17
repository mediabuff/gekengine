﻿#include "CGEKComponentSprite.h"
#include "GEKSystemCLSIDs.h"
#include "GEKEngineCLSIDs.h"
#include "GEKEngine.h"

#define NUM_INSTANCES                   50

REGISTER_COMPONENT(sprite)
    REGISTER_COMPONENT_SERIALIZE(sprite)
        REGISTER_COMPONENT_SERIALIZE_VALUE(source, )
        REGISTER_COMPONENT_SERIALIZE_VALUE(size, StrFromFloat)
        REGISTER_COMPONENT_SERIALIZE_VALUE(color, StrFromFloat4)
    REGISTER_COMPONENT_DESERIALIZE(sprite)
        REGISTER_COMPONENT_DESERIALIZE_VALUE(source, )
        REGISTER_COMPONENT_DESERIALIZE_VALUE(size, StrToFloat)
        REGISTER_COMPONENT_DESERIALIZE_VALUE(color, StrToFloat4)
END_REGISTER_COMPONENT(sprite)

BEGIN_INTERFACE_LIST(CGEKComponentSystemSprite)
    INTERFACE_LIST_ENTRY_COM(IGEKComponentSystem)
    INTERFACE_LIST_ENTRY_COM(IGEKSceneObserver)
    INTERFACE_LIST_ENTRY_COM(IGEKRenderObserver)
END_INTERFACE_LIST_UNKNOWN

REGISTER_CLASS(CGEKComponentSystemSprite)

CGEKComponentSystemSprite::CGEKComponentSystemSprite(void)
    : m_pRenderManager(nullptr)
    , m_pSceneManager(nullptr)
    , m_pVideoSystem(nullptr)
    , m_pMaterialManager(nullptr)
    , m_pProgramManager(nullptr)
{
}

CGEKComponentSystemSprite::~CGEKComponentSystemSprite(void)
{
}

STDMETHODIMP CGEKComponentSystemSprite::Initialize(void)
{
    HRESULT hRetVal = E_FAIL;
    m_pVideoSystem = GetContext()->GetCachedClass<IGEK3DVideoSystem>(CLSID_GEKVideoSystem);
    m_pSceneManager = GetContext()->GetCachedClass<IGEKSceneManager>(CLSID_GEKPopulationSystem);
    m_pRenderManager = GetContext()->GetCachedClass<IGEKRenderManager>(CLSID_GEKRenderSystem);
    m_pProgramManager = GetContext()->GetCachedClass<IGEKProgramManager>(CLSID_GEKRenderSystem);
    m_pMaterialManager = GetContext()->GetCachedClass<IGEKMaterialManager>(CLSID_GEKRenderSystem);
    if (m_pRenderManager && m_pSceneManager && m_pVideoSystem && m_pMaterialManager && m_pProgramManager)
    {
        hRetVal = CGEKObservable::AddObserver(m_pRenderManager, (IGEKRenderObserver *)GetUnknown());
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = CGEKObservable::AddObserver(m_pSceneManager, (IGEKSceneObserver *)GetUnknown());
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = E_FAIL;
        CComQIPtr<IGEKProgramManager> spProgramManager(m_pRenderManager);
        if (spProgramManager != nullptr)
        {
            hRetVal = spProgramManager->LoadProgram(L"sprite", &m_spVertexProgram);
        }
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = E_FAIL;
        IGEK3DVideoSystem *pVideoSystem = GetContext()->GetCachedClass<IGEK3DVideoSystem>(CLSID_GEKVideoSystem);
        if (pVideoSystem != nullptr)
        {
            hRetVal = pVideoSystem->CreateBuffer(sizeof(INSTANCE), NUM_INSTANCES, GEK3DVIDEO::BUFFER::DYNAMIC | GEK3DVIDEO::BUFFER::STRUCTURED_BUFFER | GEK3DVIDEO::BUFFER::RESOURCE, &m_spInstanceBuffer);
            GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
        }
    }

    if (SUCCEEDED(hRetVal))
    {
        float2 aVertices[8] =
        {
            float2(-1.0f, -1.0f), float2(0.0f, 0.0f),
            float2( 1.0f, -1.0f), float2(1.0f, 0.0f),
            float2( 1.0f,  1.0f), float2(1.0f, 1.0f),
            float2(-1.0f,  1.0f), float2(0.0f, 1.0f),
        };

        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(float4), 4, GEK3DVIDEO::BUFFER::VERTEX_BUFFER | GEK3DVIDEO::BUFFER::STATIC, &m_spVertexBuffer, aVertices);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        UINT16 aIndices[6] =
        {
            0, 1, 2,
            0, 2, 3,
        };

        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(UINT16), 6, GEK3DVIDEO::BUFFER::INDEX_BUFFER | GEK3DVIDEO::BUFFER::STATIC, &m_spIndexBuffer, aIndices);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKComponentSystemSprite::Destroy(void)
{
    CGEKObservable::RemoveObserver(m_pRenderManager, (IGEKRenderObserver *)GetUnknown());
    CGEKObservable::RemoveObserver(m_pSceneManager, (IGEKSceneObserver *)GetUnknown());
}

STDMETHODIMP CGEKComponentSystemSprite::OnLoadEnd(HRESULT hRetVal)
{
    return S_OK;
}

STDMETHODIMP_(void) CGEKComponentSystemSprite::OnFree(void)
{
}

STDMETHODIMP_(void) CGEKComponentSystemSprite::OnPreRender(void)
{
}

STDMETHODIMP_(void) CGEKComponentSystemSprite::OnCullScene(void)
{
    REQUIRE_VOID_RETURN(m_pSceneManager);

    m_pSceneManager->ListComponentsEntities({ L"transform", L"sprite" }, [&](const GEKENTITYID &nEntityID)->void
    {
        auto &kSprite = m_pSceneManager->GetComponent<GET_COMPONENT_DATA(sprite)>(nEntityID, L"sprite");

        CComPtr<IUnknown> spMaterial;
        m_pMaterialManager->LoadMaterial(kSprite.source, &spMaterial);
        if (spMaterial)
        {
            float nHalfSize = (kSprite.size * 0.5f);
            auto &kTransform = m_pSceneManager->GetComponent<GET_COMPONENT_DATA(transform)>(nEntityID, L"transform");
            if (m_pRenderManager->GetFrustum().IsVisible(aabb(kTransform.position - nHalfSize, kTransform.position + nHalfSize)))
            {
                m_aVisible[spMaterial].emplace_back(kTransform.position, nHalfSize, kSprite.color);
            }
        }
    });
}

STDMETHODIMP_(void) CGEKComponentSystemSprite::OnDrawScene(IGEK3DVideoContext *pContext, UINT32 nVertexAttributes)
{
    REQUIRE_VOID_RETURN(pContext);

    m_pProgramManager->EnableProgram(pContext, m_spVertexProgram);
    pContext->GetVertexSystem()->SetResource(0, m_spInstanceBuffer);
    pContext->SetPrimitiveType(GEK3DVIDEO::PRIMITIVE::TRIANGLELIST);
    pContext->SetVertexBuffer(0, 0, m_spVertexBuffer);
    pContext->SetIndexBuffer(0, m_spIndexBuffer);
    for (auto kModel : m_aVisible)
    {
        for (UINT32 nPass = 0; nPass < kModel.second.size(); nPass += NUM_INSTANCES)
        {
            UINT32 nNumInstances = min(NUM_INSTANCES, (kModel.second.size() - nPass));

            INSTANCE *pInstances = nullptr;
            if (SUCCEEDED(m_spInstanceBuffer->Map((LPVOID *)&pInstances)))
            {
                memcpy(pInstances, &kModel.second[nPass], (sizeof(INSTANCE) * nNumInstances));
                m_spInstanceBuffer->UnMap();

                if (m_pMaterialManager->EnableMaterial(pContext, kModel.first))
                {
                    pContext->DrawInstancedIndexedPrimitive(6, nNumInstances, 0, 0, 0);
                }
            }
        }
    }
}

STDMETHODIMP_(void) CGEKComponentSystemSprite::OnPostRender(void)
{
    m_aVisible.clear();
}
