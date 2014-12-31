﻿#include "CGEKRenderSystem.h"
#include "IGEKRenderFilter.h"
#include "CGEKProperties.h"
#include "GEKSystemCLSIDs.h"
#include "GEKEngineCLSIDs.h"
#include "GEKEngine.h"
#include <windowsx.h>
#include <atlpath.h>

DECLARE_INTERFACE_IID_(IGEKMaterial, IUnknown, "819CA201-F652-4183-B29D-BB71BB15810E")
{
    STDMETHOD_(bool, Enable)                (THIS_ CGEKRenderSystem *pManager, IGEK3DVideoContext *pContext, const std::vector<CStringW> &aData) PURE;
    STDMETHOD_(float4, GetColor)            (THIS) PURE;
    STDMETHOD_(bool, IsFullBright)          (THIS) PURE;
};

class CGEKMaterial : public CGEKUnknown
                   , public IGEKMaterial
                   , public CGEKRenderStates
                   , public CGEKBlendStates
{
private:
    std::unordered_map<CStringW, CComPtr<IUnknown>> m_aData;
    bool m_bFullBright;
    float4 m_nColor;

public:
    DECLARE_UNKNOWN(CGEKMaterial);
    CGEKMaterial(const std::unordered_map<CStringW, CComPtr<IUnknown>> &aData, const float4 &nColor, bool bFullBright)
        : m_aData(aData)
        , m_nColor(nColor)
        , m_bFullBright(bFullBright)
    {
    }

    ~CGEKMaterial(void)
    {
    }

    // IGEKMaterial
    STDMETHODIMP_(bool) Enable(CGEKRenderSystem *pManager, IGEK3DVideoContext *pContext, const std::vector<CStringW> &aData)
    {
        bool bHasData = false;
        for (UINT32 nStage = 0; nStage < aData.size(); nStage++)
        {
            auto pIterator = m_aData.find(aData[nStage]);
            if (pIterator != m_aData.end())
            {
                bHasData = true;
                pManager->SetResource(pContext->GetPixelSystem(), nStage, (*pIterator).second);
            }
            else
            {
                pManager->SetResource(pContext->GetPixelSystem(), nStage, nullptr);
            }
        }

        if (bHasData)
        {
            CGEKRenderStates::Enable(pContext);
            CGEKBlendStates::Enable(pContext);
        }

        return bHasData;
    }

    STDMETHODIMP_(float4) GetColor(void)
    {
        return m_nColor;
    }

    STDMETHODIMP_(bool) IsFullBright(void)
    {
        return m_bFullBright;
    }
};

BEGIN_INTERFACE_LIST(CGEKMaterial)
    INTERFACE_LIST_ENTRY_COM(IGEKMaterial)
END_INTERFACE_LIST_UNKNOWN

DECLARE_INTERFACE_IID_(IGEKProgram, IUnknown, "0387E446-E858-4F3C-9E19-1F0E36D914E3")
{
    STDMETHOD_(IUnknown *, GetVertexProgram)    (THIS) PURE;
    STDMETHOD_(IUnknown *, GetGeometryProgram)  (THIS) PURE;
};

class CGEKProgram : public CGEKUnknown
                  , public IGEKProgram
{
private:
    CComPtr<IUnknown> m_spVertexProgram;
    CComPtr<IUnknown> m_spGeometryProgram;

public:
    DECLARE_UNKNOWN(CGEKProgram);
    CGEKProgram(IUnknown *pVertexProgram, IUnknown *pGeometryProgram)
        : m_spVertexProgram(pVertexProgram)
        , m_spGeometryProgram(pGeometryProgram)
    {
    }

    ~CGEKProgram(void)
    {
    }

    STDMETHODIMP_(IUnknown *) GetVertexProgram(void)
    {
        return m_spVertexProgram;
    }

    STDMETHODIMP_(IUnknown *) GetGeometryProgram(void)
    {
        return (m_spGeometryProgram ? m_spGeometryProgram : nullptr);
    }
};

BEGIN_INTERFACE_LIST(CGEKProgram)
    INTERFACE_LIST_ENTRY_COM(IGEKProgram)
END_INTERFACE_LIST_UNKNOWN

static GEK3DVIDEO::DATA::FORMAT GetFormatType(LPCWSTR pValue)
{
         if (_wcsicmp(pValue, L"R_FLOAT") == 0) return GEK3DVIDEO::DATA::R_FLOAT;
    else if (_wcsicmp(pValue, L"RG_FLOAT") == 0) return GEK3DVIDEO::DATA::RG_FLOAT;
    else if (_wcsicmp(pValue, L"RGB_FLOAT") == 0) return GEK3DVIDEO::DATA::RGB_FLOAT;
    else if (_wcsicmp(pValue, L"RGBA_FLOAT") == 0) return GEK3DVIDEO::DATA::RGBA_FLOAT;
    else if (_wcsicmp(pValue, L"R_UINT32") == 0) return GEK3DVIDEO::DATA::R_UINT32;
    else if (_wcsicmp(pValue, L"RG_UINT32") == 0) return GEK3DVIDEO::DATA::RG_UINT32;
    else if (_wcsicmp(pValue, L"RGB_UINT32") == 0) return GEK3DVIDEO::DATA::RGB_UINT32;
    else if (_wcsicmp(pValue, L"RGBA_UINT32") == 0) return GEK3DVIDEO::DATA::RGBA_UINT32;
    else return GEK3DVIDEO::DATA::UNKNOWN;
}

static GEK3DVIDEO::INPUT::SOURCE GetElementClass(LPCWSTR pValue)
{
         if (_wcsicmp(pValue, L"vertex") == 0) return GEK3DVIDEO::INPUT::VERTEX;
    else if (_wcsicmp(pValue, L"instance") == 0) return GEK3DVIDEO::INPUT::INSTANCE;
    else return GEK3DVIDEO::INPUT::UNKNOWN;
}

BEGIN_INTERFACE_LIST(CGEKRenderSystem)
    INTERFACE_LIST_ENTRY_COM(IGEKObservable)
    INTERFACE_LIST_ENTRY_COM(IGEK3DVideoObserver)
    INTERFACE_LIST_ENTRY_COM(IGEKSceneObserver)
    INTERFACE_LIST_ENTRY_COM(IGEKRenderSystem)
    INTERFACE_LIST_ENTRY_COM(IGEKProgramManager)
    INTERFACE_LIST_ENTRY_COM(IGEKMaterialManager)
END_INTERFACE_LIST_UNKNOWN

REGISTER_CLASS(CGEKRenderSystem)

CGEKRenderSystem::CGEKRenderSystem(void)
    : m_pSystem(nullptr)
    , m_pVideoSystem(nullptr)
    , m_pEngine(nullptr)
    , m_pSceneManager(nullptr)
    , m_pCurrentPass(nullptr)
    , m_pCurrentFilter(nullptr)
    , m_nNumLightInstances(254)
{
}

CGEKRenderSystem::~CGEKRenderSystem(void)
{
}

STDMETHODIMP CGEKRenderSystem::Initialize(void)
{
    GEKFUNCTION(nullptr);
    HRESULT hRetVal = GetContext()->AddCachedClass(CLSID_GEKRenderSystem, GetUnknown());
    if (SUCCEEDED(hRetVal))
    {
        m_pSystem = GetContext()->GetCachedClass<IGEKSystem>(CLSID_GEKSystem);
        m_pVideoSystem = GetContext()->GetCachedClass<IGEK3DVideoSystem>(CLSID_GEKVideoSystem);
        m_pEngine = GetContext()->GetCachedClass<IGEKEngine>(CLSID_GEKEngine);
        m_pSceneManager = GetContext()->GetCachedClass<IGEKSceneManager>(CLSID_GEKPopulationSystem);
        if (m_pSystem == nullptr ||
            m_pVideoSystem == nullptr ||
            m_pEngine == nullptr ||
            m_pSceneManager == nullptr)
        {
            hRetVal = E_FAIL;
        }
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = CGEKObservable::AddObserver(m_pVideoSystem, (IGEK3DVideoObserver *)GetUnknown());
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = CGEKObservable::AddObserver(m_pSceneManager, (IGEKSceneObserver *)GetUnknown());
    }

    if (SUCCEEDED(hRetVal))
    {
        std::vector<GEK3DVIDEO::INPUTELEMENT> aLayout;
        aLayout.push_back(GEK3DVIDEO::INPUTELEMENT(GEK3DVIDEO::DATA::RG_FLOAT, "POSITION", 0));
        aLayout.push_back(GEK3DVIDEO::INPUTELEMENT(GEK3DVIDEO::DATA::RG_FLOAT, "TEXCOORD", 0));
        hRetVal = m_pVideoSystem->LoadVertexProgram(L"%root%\\data\\programs\\vertex\\overlay.hlsl", "MainVertexProgram", aLayout, &m_spVertexProgram);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to LoadVertexProgram failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = m_pVideoSystem->LoadPixelProgram(L"%root%\\data\\programs\\pixel\\overlay.hlsl", "MainPixelProgram", &m_spPixelProgram);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to LoadPixelProgram failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        float2 aVertices[] =
        {
            float2(0.0f, 0.0f),
            float2(1.0f, 0.0f),
            float2(1.0f, 1.0f),
            float2(0.0f, 1.0f),
        };

        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(float2), 4, GEK3DVIDEO::BUFFER::VERTEX_BUFFER | GEK3DVIDEO::BUFFER::STATIC, &m_spVertexBuffer, aVertices);
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

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(float4x4), 1, GEK3DVIDEO::BUFFER::CONSTANT_BUFFER, &m_spOrthoBuffer);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
        if (m_spOrthoBuffer)
        {
            float4x4 nOverlayMatrix;
            nOverlayMatrix.SetOrthographic(0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f);
            m_spOrthoBuffer->Update((void *)&nOverlayMatrix);
        }
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(ENGINEBUFFER), 1, GEK3DVIDEO::BUFFER::CONSTANT_BUFFER, &m_spEngineBuffer);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(MATERIALBUFFER), 1, GEK3DVIDEO::BUFFER::CONSTANT_BUFFER, &m_spMaterialBuffer);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(UINT32) * 4, 1, GEK3DVIDEO::BUFFER::CONSTANT_BUFFER, &m_spLightCountBuffer);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = m_pVideoSystem->CreateBuffer(sizeof(LIGHTBUFFER), m_nNumLightInstances, GEK3DVIDEO::BUFFER::DYNAMIC | GEK3DVIDEO::BUFFER::STRUCTURED_BUFFER | GEK3DVIDEO::BUFFER::RESOURCE, &m_spLightBuffer);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateBuffer failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        GEK3DVIDEO::SAMPLERSTATES kStates;
        kStates.m_eFilter = GEK3DVIDEO::FILTER::MIN_MAG_MIP_POINT;
        kStates.m_eAddressU = GEK3DVIDEO::ADDRESS::CLAMP;
        kStates.m_eAddressV = GEK3DVIDEO::ADDRESS::CLAMP;
        hRetVal = m_pVideoSystem->CreateSamplerStates(kStates, &m_spPointSampler);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateSamplerStates failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        GEK3DVIDEO::SAMPLERSTATES kStates;
        if (m_pSystem->GetConfig().DoesValueExists(L"render", L"anisotropy"))
        {
            kStates.m_nMaxAnisotropy = StrToUINT32(m_pSystem->GetConfig().GetValue(L"render", L"anisotropy", L"1"));
            kStates.m_eFilter = GEK3DVIDEO::FILTER::ANISOTROPIC;
        }
        else
        {
            kStates.m_eFilter = GEK3DVIDEO::FILTER::MIN_MAG_MIP_LINEAR;
        }

        kStates.m_eAddressU = GEK3DVIDEO::ADDRESS::WRAP;
        kStates.m_eAddressV = GEK3DVIDEO::ADDRESS::WRAP;
        hRetVal = m_pVideoSystem->CreateSamplerStates(kStates, &m_spLinearSampler);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateSamplerStates failed: 0x%08X", hRetVal);
    }

    if (SUCCEEDED(hRetVal))
    {
        GEK3DVIDEO::RENDERSTATES kRenderStates;
        hRetVal = m_pVideoSystem->CreateRenderStates(kRenderStates, &m_spRenderStates);
    }

    if (SUCCEEDED(hRetVal))
    {
        GEK3DVIDEO::UNIFIEDBLENDSTATES kBlendStates;
        hRetVal = m_pVideoSystem->CreateBlendStates(kBlendStates, &m_spBlendStates);
    }
   
    if (SUCCEEDED(hRetVal))
    {
        GEK3DVIDEO::DEPTHSTATES kDepthStates;
        hRetVal = m_pVideoSystem->CreateDepthStates(kDepthStates, &m_spDepthStates);
    }

    if (SUCCEEDED(hRetVal))
    {
        hRetVal = m_pVideoSystem->CreateEvent(&m_spFrameEvent);
        GEKRESULT(SUCCEEDED(hRetVal), L"Call to CreateEvent failed: 0x%08X", hRetVal);
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKRenderSystem::Destroy(void)
{
    m_pCurrentPass = nullptr;
    m_pCurrentFilter = nullptr;
    m_aVisibleLights.clear();
    m_aResources.clear();
    m_aBuffers.clear();
    m_aPasses.clear();

    GetContext()->RemoveCachedObserver(CLSID_GEKVideoSystem, (IGEK3DVideoObserver *)GetUnknown());
    GetContext()->RemoveCachedObserver(CLSID_GEKPopulationSystem, (IGEKSceneObserver *)GetUnknown());
    GetContext()->RemoveCachedClass(CLSID_GEKRenderSystem);
}

STDMETHODIMP_(void) CGEKRenderSystem::OnPreReset(void)
{
    for (auto &kBuffer : m_aBuffers)
    {
        kBuffer.second.m_spResource = nullptr;
    }

    for (auto &kPass : m_aPasses)
    {
        kPass.second.m_aBuffers[0] = nullptr;
        kPass.second.m_aBuffers[1] = nullptr;
    }
}

STDMETHODIMP CGEKRenderSystem::OnPostReset(void)
{
    HRESULT hRetVal = E_FAIL;
    IGEKSystem *pSystem = GetContext()->GetCachedClass<IGEKSystem>(CLSID_GEKSystem);
    if (pSystem != nullptr)
    {
        hRetVal = S_OK;
        for (auto &kBuffer : m_aBuffers)
        {
            if (kBuffer.second.m_eFormat != GEK3DVIDEO::DATA::UNKNOWN)
            {
                switch (kBuffer.second.m_eFormat)
                {
                case GEK3DVIDEO::DATA::D16:
                case GEK3DVIDEO::DATA::D24_S8:
                case GEK3DVIDEO::DATA::D32:
                    hRetVal = m_pVideoSystem->CreateDepthTarget(kBuffer.second.m_nXSize, kBuffer.second.m_nYSize, kBuffer.second.m_eFormat, &kBuffer.second.m_spResource);
                    break;

                default:
                    if (true)
                    {
                        CComPtr<IGEK3DVideoTexture> spTarget;
                        hRetVal = m_pVideoSystem->CreateRenderTarget(kBuffer.second.m_nXSize, kBuffer.second.m_nYSize, kBuffer.second.m_eFormat, &spTarget);
                        if (spTarget)
                        {
                            hRetVal = spTarget->QueryInterface(IID_PPV_ARGS(&kBuffer.second.m_spResource));
                        }
                    }

                    break;
                };

                if (FAILED(hRetVal))
                {
                    break;
                }
            }
        }

        for (auto &kPass : m_aPasses)
        {
            if (SUCCEEDED(hRetVal))
            {
                hRetVal = m_pVideoSystem->CreateRenderTarget(kPass.second.m_nXSize, kPass.second.m_nYSize, GEK3DVIDEO::DATA::RGBA_UINT8, &kPass.second.m_aBuffers[0]);
            }

            if (SUCCEEDED(hRetVal))
            {
                hRetVal = m_pVideoSystem->CreateRenderTarget(kPass.second.m_nXSize, kPass.second.m_nYSize, GEK3DVIDEO::DATA::RGBA_UINT8, &kPass.second.m_aBuffers[1]);
            }

            if (FAILED(hRetVal))
            {
                break;
            }
        }
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKRenderSystem::OnLoadBegin(void)
{
    m_aResources.clear();
    m_aBuffers.clear();
    m_aPasses.clear();
}

STDMETHODIMP CGEKRenderSystem::OnLoadEnd(HRESULT hRetVal)
{
    if (SUCCEEDED(hRetVal))
    {
        m_pVideoSystem->SetEvent(m_spFrameEvent);
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKRenderSystem::OnFree(void)
{
    m_aResources.clear();
    m_aBuffers.clear();
    m_aPasses.clear();
    m_pCurrentPass = nullptr;
    m_pCurrentFilter = nullptr;
    m_aVisibleLights.clear();
}

HRESULT CGEKRenderSystem::LoadPass(LPCWSTR pName)
{
    REQUIRE_RETURN(pName, E_INVALIDARG);

    HRESULT hRetVal = E_FAIL;
    auto pPassIterator = m_aPasses.find(pName);
    if (pPassIterator != m_aPasses.end())
    {
        hRetVal = S_OK;
    }
    else
    {
        GEKFUNCTION(L"Name(%s)", pName);

        CLibXMLDoc kDocument;
        CStringW strFileName(FormatString(L"%%root%%\\data\\passes\\%s.xml", pName));
        hRetVal = kDocument.Load(strFileName);
        if (SUCCEEDED(hRetVal))
        {
            PASS kPassData;
            hRetVal = E_INVALID;
            CLibXMLNode kPassNode = kDocument.GetRoot();
            if (kPassNode)
            {
                hRetVal = E_INVALID;
                CLibXMLNode kMaterialsNode = kPassNode.FirstChildElement(L"materials");
                if (kMaterialsNode)
                {
                    CLibXMLNode kDataNode = kMaterialsNode.FirstChildElement(L"data");
                    while (kDataNode)
                    {
                        CStringW strFilter(kDataNode.GetAttribute(L"source"));
                        GEKLOG(L"Data Source: %s", strFilter.GetString());
                        kPassData.m_aData.push_back(strFilter);
                        kDataNode = kDataNode.NextSiblingElement(L"data");
                    };
                }

                CLibXMLNode kFiltersNode = kPassNode.FirstChildElement(L"filters");
                if (kFiltersNode)
                {
                    CLibXMLNode kFilterNode = kFiltersNode.FirstChildElement(L"filter");
                    while (kFilterNode)
                    {
                        hRetVal = S_OK;
                        std::unordered_map<CStringA, CStringA> aDefines;
                        CLibXMLNode kDefinesNode = kFilterNode.FirstChildElement(L"defines");
                        if (kDefinesNode)
                        {
                            CLibXMLNode kDefineNode = kDefinesNode.FirstChildElement(L"define");
                            while (kDefineNode)
                            {
                                if (kDefineNode.HasAttribute(L"name") &&
                                    kDefineNode.HasAttribute(L"value"))
                                {
                                    CStringA strName = CW2A(kDefineNode.GetAttribute(L"name"), CP_UTF8);
                                    CStringA strValue = CW2A(kDefineNode.GetAttribute(L"value"), CP_UTF8);

                                    aDefines[strName] = strValue;
                                    kDefineNode = kDefineNode.NextSiblingElement(L"define");
                                }
                                else
                                {
                                    hRetVal = E_INVALIDARG;
                                    break;
                                }
                            };
                        }

                        if (SUCCEEDED(hRetVal))
                        {
                            CStringW strFilter(kFilterNode.GetAttribute(L"source"));
                            GEKLOG(L"Filter Source: %s", strFilter.GetString());

                            CComPtr<IGEKRenderFilter> spFilter;
                            hRetVal = GetContext()->CreateInstance(CLSID_GEKRenderFilter, IID_PPV_ARGS(&spFilter));
                            if (spFilter)
                            {
                                CStringW strFilterFileName(L"%root%\\data\\filters\\" + strFilter + L".xml");
                                hRetVal = spFilter->Load(strFilterFileName, aDefines);
                                if (SUCCEEDED(hRetVal))
                                {
                                    kPassData.m_aFilters.push_back(spFilter);
                                }
                                else
                                {
                                    break;
                                }
                            }

                            kFilterNode = kFilterNode.NextSiblingElement(L"filter");
                        }
                    };
                }
            }

            if (SUCCEEDED(hRetVal))
            {
                kPassData.m_nXSize = m_pSystem->GetXSize();
                kPassData.m_nYSize = m_pSystem->GetYSize();
                hRetVal = m_pVideoSystem->CreateRenderTarget(kPassData.m_nXSize, kPassData.m_nYSize, GEK3DVIDEO::DATA::RGBA_UINT8, &kPassData.m_aBuffers[0]);
                if (SUCCEEDED(hRetVal))
                {
                    hRetVal = m_pVideoSystem->CreateRenderTarget(kPassData.m_nXSize, kPassData.m_nYSize, GEK3DVIDEO::DATA::RGBA_UINT8, &kPassData.m_aBuffers[1]);
                }
            }

            if (SUCCEEDED(hRetVal))
            {
                m_aPasses[pName] = kPassData;
            }
        }
    }

    return hRetVal;
}

STDMETHODIMP CGEKRenderSystem::LoadResource(LPCWSTR pName, IUnknown **ppResource)
{
    REQUIRE_RETURN(pName && ppResource, E_INVALIDARG);

    HRESULT hRetVal = E_FAIL;
    auto pIterator = m_aResources.find(pName);
    if (pIterator != m_aResources.end())
    {
        hRetVal = (*pIterator).second->QueryInterface(IID_PPV_ARGS(ppResource));
    }
    else
    {
        GEKFUNCTION(L"Name(%s)", pName);
        CComPtr<IUnknown> spTexture;
        if (pName[0] == L'*')
        {
            int nPosition = 0;
            CStringW strName(&pName[1]);
            CStringW strType(strName.Tokenize(L":", nPosition));
            if (strType.CompareNoCase(L"color") == 0)
            {
                CStringW strColor(strName.Tokenize(L":", nPosition));
                GEKLOG(L"Creating Color Texture: %s", strColor.GetString());
                float4 nColor = StrToFloat4(strColor);

                CComPtr<IGEK3DVideoTexture> spColorTexture;
                hRetVal = m_pVideoSystem->CreateTexture(1, 1, 1, GEK3DVIDEO::DATA::RGBA_UINT8, GEK3DVIDEO::TEXTURE::RESOURCE, &spColorTexture);
                if (spColorTexture)
                {
                    UINT32 nColorValue = UINT32(UINT8(nColor.r * 255.0f)) |
                                         UINT32(UINT8(nColor.g * 255.0f) << 8) |
                                         UINT32(UINT8(nColor.b * 255.0f) << 16) |
                                         UINT32(UINT8(nColor.a * 255.0f) << 24);
                    m_pVideoSystem->UpdateTexture(spColorTexture, &nColorValue, 4);
                    spTexture = spColorTexture;
                }
            }
        }
        else
        {
            GEKLOG(L"Loading Texture: %s", pName);
            CComPtr<IGEK3DVideoTexture> spFileTexture;
            hRetVal = m_pVideoSystem->LoadTexture(FormatString(L"%%root%%\\data\\textures\\%s", pName), &spFileTexture);
            if (spFileTexture)
            {
                spTexture = spFileTexture;
            }
        }

        if (spTexture)
        {
            spTexture->QueryInterface(IID_PPV_ARGS(&m_aResources[pName]));
            hRetVal = spTexture->QueryInterface(IID_PPV_ARGS(ppResource));
        }
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKRenderSystem::SetResource(IGEK3DVideoContextSystem *pSystem, UINT32 nStage, IUnknown *pResource)
{
    REQUIRE_VOID_RETURN(pSystem);
    CComPtr<IUnknown> spResource(pResource);
    if (spResource)
    {
        pSystem->SetResource(nStage, spResource);
    }
}

STDMETHODIMP CGEKRenderSystem::LoadBuffer(LPCWSTR pName, UINT32 nStride, UINT32 nCount)
{
    REQUIRE_RETURN(m_pVideoSystem, E_FAIL);
    REQUIRE_RETURN(pName, E_INVALIDARG);

    HRESULT hRetVal = S_OK;
    if (m_aBuffers.find(pName) == m_aBuffers.end())
    {
        CComPtr<IGEK3DVideoBuffer> spBuffer;
        hRetVal = m_pVideoSystem->CreateBuffer(nStride, nCount, GEK3DVIDEO::BUFFER::STRUCTURED_BUFFER | GEK3DVIDEO::BUFFER::RESOURCE, &spBuffer);
        if (spBuffer)
        {
            BUFFER &kBuffer = m_aBuffers[pName];
            kBuffer.m_nStride = nStride;
            kBuffer.m_nCount = nCount;
            kBuffer.m_spResource = spBuffer;
        }
    }

    return hRetVal;
}

STDMETHODIMP CGEKRenderSystem::LoadBuffer(LPCWSTR pName, GEK3DVIDEO::DATA::FORMAT eFormat, UINT32 nCount)
{
    REQUIRE_RETURN(m_pVideoSystem, E_FAIL);
    REQUIRE_RETURN(pName, E_INVALIDARG);

    HRESULT hRetVal = S_OK;
    if (m_aBuffers.find(pName) == m_aBuffers.end())
    {
        CComPtr<IGEK3DVideoBuffer> spBuffer;
        hRetVal = m_pVideoSystem->CreateBuffer(eFormat, nCount, GEK3DVIDEO::BUFFER::UNORDERED_ACCESS | GEK3DVIDEO::BUFFER::RESOURCE, &spBuffer);
        if (spBuffer)
        {
            BUFFER &kBuffer = m_aBuffers[pName];
            kBuffer.m_eFormat = eFormat;
            kBuffer.m_nCount = nCount;
            kBuffer.m_spResource = spBuffer;
        }
    }

    return hRetVal;
}

STDMETHODIMP CGEKRenderSystem::LoadBuffer(LPCWSTR pName, UINT32 nXSize, UINT32 nYSize, GEK3DVIDEO::DATA::FORMAT eFormat)
{
    REQUIRE_RETURN(m_pVideoSystem, E_FAIL);
    REQUIRE_RETURN(pName, E_INVALIDARG);

    HRESULT hRetVal = S_OK;
    if (m_aBuffers.find(pName) == m_aBuffers.end())
    {
        CComPtr<IUnknown> spResource;
        switch (eFormat)
        {
        case GEK3DVIDEO::DATA::D16:
        case GEK3DVIDEO::DATA::D24_S8:
        case GEK3DVIDEO::DATA::D32:
            hRetVal = m_pVideoSystem->CreateDepthTarget(nXSize, nYSize, eFormat, &spResource);
            break;

        default:
            if (true)
            {
                CComPtr<IGEK3DVideoTexture> spTarget;
                hRetVal = m_pVideoSystem->CreateRenderTarget(nXSize, nYSize, eFormat, &spTarget);
                if (spTarget)
                {
                    hRetVal = spTarget->QueryInterface(IID_PPV_ARGS(&spResource));
                }
            }

            break;
        };

        if (spResource)
        {
            BUFFER &kBuffer = m_aBuffers[pName];
            kBuffer.m_nXSize = nXSize;
            kBuffer.m_nYSize = nYSize;
            kBuffer.m_eFormat = eFormat;
            kBuffer.m_spResource = spResource;
        }
    }

    return hRetVal;
}

STDMETHODIMP CGEKRenderSystem::GetBuffer(LPCWSTR pName, IUnknown **ppResource)
{
    REQUIRE_RETURN(m_pCurrentPass, E_FAIL);
    REQUIRE_RETURN(ppResource, E_INVALIDARG);

    HRESULT hRetVal = E_FAIL;
    if (_wcsicmp(pName, L"output") == 0)
    {
        hRetVal = m_pCurrentPass->m_aBuffers[!m_pCurrentPass->m_nCurrentBuffer]->QueryInterface(IID_PPV_ARGS(ppResource));
    }
    else
    {
        auto pIterator = m_aBuffers.find(pName);
        if (pIterator != m_aBuffers.end())
        {
            hRetVal = (*pIterator).second.m_spResource->QueryInterface(IID_PPV_ARGS(ppResource));
        }
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKRenderSystem::FlipCurrentBuffers(void)
{
    REQUIRE_VOID_RETURN(m_pCurrentPass);
    m_pCurrentPass->m_nCurrentBuffer = !m_pCurrentPass->m_nCurrentBuffer;
}

STDMETHODIMP_(void) CGEKRenderSystem::SetScreenTargets(IGEK3DVideoContext *pContext, IUnknown *pDepthBuffer)
{
    REQUIRE_VOID_RETURN(m_pSystem && m_pCurrentPass);

    pContext->SetRenderTargets({ m_pCurrentPass->m_aBuffers[m_pCurrentPass->m_nCurrentBuffer] }, (pDepthBuffer ? pDepthBuffer : nullptr));

    GEK3DVIDEO::VIEWPORT kViewport;
    kViewport.m_nTopLeftX = 0.0f;
    kViewport.m_nTopLeftY = 0.0f;
    kViewport.m_nXSize = float(m_pCurrentPass->m_nXSize);
    kViewport.m_nYSize = float(m_pCurrentPass->m_nYSize);
    kViewport.m_nMinDepth = 0.0f;
    kViewport.m_nMaxDepth = 1.0f;
    pContext->SetViewports({ kViewport });
}

STDMETHODIMP CGEKRenderSystem::LoadMaterial(LPCWSTR pName, IUnknown **ppMaterial)
{
    REQUIRE_RETURN(pName && ppMaterial, E_INVALIDARG);

    HRESULT hRetVal = E_FAIL;
    auto pIterator = m_aResources.find(pName);
    if (pIterator != m_aResources.end())
    {
        hRetVal = (*pIterator).second->QueryInterface(IID_PPV_ARGS(ppMaterial));
    }
    else
    {
        GEKFUNCTION(L"Name(%s)", pName);

        CLibXMLDoc kDocument;
        hRetVal = kDocument.Load(FormatString(L"%%root%%\\data\\materials\\%s.xml", pName));
        if (SUCCEEDED(hRetVal))
        {
            hRetVal = E_INVALIDARG;
            CLibXMLNode kMaterialNode = kDocument.GetRoot();
            if (kMaterialNode)
            {
                CLibXMLNode kRenderNode = kMaterialNode.FirstChildElement(L"render");
                if (kRenderNode)
                {
                    CPathW kDirectory(pName);
                    kDirectory.RemoveFileSpec();

                    bool bFullBright = false;
                    if (kRenderNode.HasAttribute(L"fullbright"))
                    {
                        bFullBright = StrToBoolean(kRenderNode.GetAttribute(L"fullbright"));
                        GEKLOG(L"Fullbright: %s", bFullBright ? L"on" : L"off");
                    }

                    float4 nColor(1.0f, 1.0f, 1.0f, 1.0f);
                    if (kRenderNode.HasAttribute(L"color"))
                    {
                        nColor = StrToFloat4(kRenderNode.GetAttribute(L"color"));
                        GEKLOG(L"Color: %s", kRenderNode.GetAttribute(L"color"));
                    }

                    std::unordered_map<CStringW, CComPtr<IUnknown>> aData;
                    CLibXMLNode kLayersNode = kRenderNode.FirstChildElement(L"layers");
                    if (kLayersNode)
                    {
                        CLibXMLNode kLayerNode = kLayersNode.FirstChildElement();
                        while (kLayerNode)
                        {
                            CStringW strSource(kLayerNode.GetAttribute(L"source"));
                            strSource.Replace(L"%material%", pName);
                            strSource.Replace(L"%directory%", kDirectory.m_strPath.GetString());
                            GEKLOG(L"%s: %s", kLayerNode.GetType().GetString(), strSource.GetString());

                            CComPtr<IUnknown> spData;
                            LoadResource(strSource, &spData);
                            if (spData)
                            {
                                aData[kLayerNode.GetType()] = spData;
                            }

                            kLayerNode = kLayerNode.NextSiblingElement();
                        };
                    }

                    CComPtr<CGEKMaterial> spMaterial(new CGEKMaterial(aData, nColor, bFullBright));
                    GEKRESULT(spMaterial, L"Unable to allocate new material instance");
                    if (spMaterial)
                    {
                        spMaterial->CGEKRenderStates::Load(m_pVideoSystem, kRenderNode.FirstChildElement(L"properties"));
                        spMaterial->CGEKBlendStates::Load(m_pVideoSystem, kRenderNode.FirstChildElement(L"blend"));
                        spMaterial->QueryInterface(IID_PPV_ARGS(&m_aResources[pName]));
                        hRetVal = spMaterial->QueryInterface(IID_PPV_ARGS(ppMaterial));
                    }
                }
            }
        }
    }

    return hRetVal;
}

STDMETHODIMP_(bool) CGEKRenderSystem::EnableMaterial(IGEK3DVideoContext *pContext, IUnknown *pMaterial)
{
    REQUIRE_RETURN(pContext && pMaterial, false);

    bool bEnabled = false;
    CComQIPtr<IGEKMaterial> spMaterial(pMaterial);
    if (spMaterial)
    {
        if (spMaterial->Enable(this, pContext, m_pCurrentPass->m_aData))
        {
            MATERIALBUFFER kMaterial;
            kMaterial.m_nColor = spMaterial->GetColor();
            kMaterial.m_bFullBright = spMaterial->IsFullBright();
            m_spMaterialBuffer->Update((void *)&kMaterial);
            bEnabled = true;
        }
    }

    return bEnabled;
}

STDMETHODIMP CGEKRenderSystem::LoadProgram(LPCWSTR pName, IUnknown **ppProgram)
{
    REQUIRE_RETURN(pName && ppProgram, E_INVALIDARG);

    HRESULT hRetVal = E_FAIL;
    auto pIterator = m_aResources.find(pName);
    if (pIterator != m_aResources.end())
    {
        hRetVal = (*pIterator).second->QueryInterface(IID_PPV_ARGS(ppProgram));
    }
    else
    {
        GEKFUNCTION(L"Name(%s)", pName);

        CStringA strDeferredProgram;
        hRetVal = GEKLoadFromFile(L"%root%\\data\\programs\\vertex\\plugin.hlsl", strDeferredProgram);
        if (SUCCEEDED(hRetVal))
        {
            if (strDeferredProgram.Find("_INSERT_WORLD_PROGRAM") < 0)
            {
                hRetVal = E_INVALID;
            }
            else
            {
                CLibXMLDoc kDocument;
                hRetVal = kDocument.Load(FormatString(L"%%root%%\\data\\programs\\vertex\\%s.xml", pName));
                if (SUCCEEDED(hRetVal))
                {
                    hRetVal = E_INVALIDARG;
                    CLibXMLNode kProgramNode = kDocument.GetRoot();
                    if (kProgramNode)
                    {
                        CLibXMLNode kLayoutNode = kProgramNode.FirstChildElement(L"layout");
                        if (kLayoutNode)
                        {
                            std::vector<CStringA> aNames;
                            std::vector<GEK3DVIDEO::INPUTELEMENT> aLayout;
                            CLibXMLNode kElementNode = kLayoutNode.FirstChildElement(L"element");
                            while (kElementNode)
                            {
                                if (kElementNode.HasAttribute(L"type") && 
                                   kElementNode.HasAttribute(L"name") &&
                                   kElementNode.HasAttribute(L"index"))
                                {
                                    aNames.push_back((LPCSTR)CW2A(kElementNode.GetAttribute(L"name")));

                                    GEK3DVIDEO::INPUTELEMENT kData;
                                    kData.m_eType = GetFormatType(kElementNode.GetAttribute(L"type"));
                                    kData.m_pName = aNames.back().GetString();
                                    kData.m_nIndex = StrToUINT32(kElementNode.GetAttribute(L"index"));
                                    if (kElementNode.HasAttribute(L"class") &&
                                       kElementNode.HasAttribute(L"slot"))
                                    {
                                        kData.m_eClass = GetElementClass(kElementNode.GetAttribute(L"class"));
                                        kData.m_nSlot = StrToUINT32(kElementNode.GetAttribute(L"slot"));
                                    }

                                    aLayout.push_back(kData);
                                }
                                else
                                {
                                    break;
                                }

                                kElementNode = kElementNode.NextSiblingElement(L"element");
                            };

                            hRetVal = S_OK;
                            CComPtr<IUnknown> spGeometryProgram;
                            CLibXMLNode kGeometryNode = kProgramNode.FirstChildElement(L"geometry");
                            if (kGeometryNode)
                            {
                                CStringA strGeometryProgram = kGeometryNode.GetText();
                                hRetVal = m_pVideoSystem->CompileGeometryProgram(strGeometryProgram, "MainGeometryProgram", &spGeometryProgram);
                            }

                            if (SUCCEEDED(hRetVal))
                            {
                                hRetVal = E_INVALIDARG;
                                CLibXMLNode kVertexNode = kProgramNode.FirstChildElement(L"vertex");
                                if (kVertexNode)
                                {
                                    CStringA strVertexProgram = kVertexNode.GetText();
                                    strDeferredProgram.Replace("_INSERT_WORLD_PROGRAM", (strVertexProgram + "\r\n"));

                                    CComPtr<IUnknown> spVertexProgram;
                                    hRetVal = m_pVideoSystem->CompileVertexProgram(strDeferredProgram, "MainVertexProgram", aLayout, &spVertexProgram);
                                    if (spVertexProgram)
                                    {
                                        CComPtr<CGEKProgram> spProgram(new CGEKProgram(spVertexProgram, spGeometryProgram));
                                        GEKRESULT(spProgram, L"Unable to allocate new program instance");
                                        if (spProgram)
                                        {
                                            spProgram->QueryInterface(IID_PPV_ARGS(&m_aResources[pName]));
                                            hRetVal = spProgram->QueryInterface(IID_PPV_ARGS(ppProgram));
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKRenderSystem::EnableProgram(IGEK3DVideoContext *pContext, IUnknown *pProgram)
{
    REQUIRE_VOID_RETURN(m_pVideoSystem);
    REQUIRE_VOID_RETURN(pProgram);

    CComQIPtr<IGEKProgram> spProgram(pProgram);
    if (spProgram)
    {
        pContext->GetVertexSystem()->SetProgram(spProgram->GetVertexProgram());
        pContext->GetGeometrySystem()->SetProgram(spProgram->GetGeometryProgram());
    }
}

STDMETHODIMP_(void) CGEKRenderSystem::DrawScene(IGEK3DVideoContext *pContext, UINT32 nAttributes)
{
    CGEKObservable::SendEvent(TGEKEvent<IGEKRenderObserver>(std::bind(&IGEKRenderObserver::OnDrawScene, std::placeholders::_1, pContext, nAttributes)));
}

STDMETHODIMP_(void) CGEKRenderSystem::DrawLights(IGEK3DVideoContext *pContext, std::function<void(void)> OnLightBatch)
{
    pContext->GetVertexSystem()->SetProgram(m_spVertexProgram);
    pContext->GetGeometrySystem()->SetProgram(nullptr);
    pContext->GetPixelSystem()->SetResource(0, m_spLightBuffer);
    pContext->GetComputeSystem()->SetResource(0, m_spLightBuffer);

    pContext->SetVertexBuffer(0, 0, m_spVertexBuffer);
    pContext->SetIndexBuffer(0, m_spIndexBuffer);
    pContext->SetPrimitiveType(GEK3DVIDEO::PRIMITIVE::TRIANGLELIST);

    for (UINT32 nPass = 0; nPass < m_aVisibleLights.size(); nPass += m_nNumLightInstances)
    {
        UINT32 nNumLights = min(m_nNumLightInstances, (m_aVisibleLights.size() - nPass));
        if (nNumLights > 1)
        {
            UINT32 aCounts[4] =
            {
                nNumLights, 0, 0, 0,
            };

            m_spLightCountBuffer->Update(aCounts);

            LIGHTBUFFER *pLights = nullptr;
            if (SUCCEEDED(m_spLightBuffer->Map((LPVOID *)&pLights)))
            {
                memcpy(pLights, &m_aVisibleLights[nPass], (sizeof(LIGHTBUFFER)* nNumLights));
                m_spLightBuffer->UnMap();

                OnLightBatch();

                pContext->DrawIndexedPrimitive(6, 0, 0);
            }
        }
    }
}

STDMETHODIMP_(void) CGEKRenderSystem::DrawOverlay(IGEK3DVideoContext *pContext)
{
    pContext->GetVertexSystem()->SetProgram(m_spVertexProgram);

    pContext->GetGeometrySystem()->SetProgram(nullptr);

    pContext->SetVertexBuffer(0, 0, m_spVertexBuffer);
    pContext->SetIndexBuffer(0, m_spIndexBuffer);
    pContext->SetPrimitiveType(GEK3DVIDEO::PRIMITIVE::TRIANGLELIST);

    pContext->DrawIndexedPrimitive(6, 0, 0);
}

STDMETHODIMP_(void) CGEKRenderSystem::Render(void)
{
    REQUIRE_VOID_RETURN(m_pSceneManager && m_pVideoSystem);
    GEKFUNCTION(nullptr);

    CComQIPtr<IGEK3DVideoContext> spContext(m_pVideoSystem);
    spContext->GetPixelSystem()->SetSamplerStates(0, m_spPointSampler);
    spContext->GetPixelSystem()->SetSamplerStates(1, m_spLinearSampler);
    m_pSceneManager->ListComponentsEntities({ L"transform", L"viewer" }, [&](const GEKENTITYID &nViewerID)->void
    {
        auto &kViewer = m_pSceneManager->GetComponent<GET_COMPONENT_DATA(viewer)>(nViewerID, L"viewer");
        if (SUCCEEDED(LoadPass(kViewer.pass)))
        {
            CGEKObservable::SendEvent(TGEKEvent<IGEKRenderObserver>(std::bind(&IGEKRenderObserver::OnPreRender, std::placeholders::_1)));

            float4x4 nCameraMatrix;
            auto &kTransform = m_pSceneManager->GetComponent<GET_COMPONENT_DATA(transform)>(nViewerID, L"transform");
            nCameraMatrix = kTransform.rotation;
            nCameraMatrix.t = kTransform.position;

            float nXSize = float(m_pSystem->GetXSize());
            float nYSize = float(m_pSystem->GetYSize());
            float nAspect = (nXSize / nYSize);

            float nFieldOfView = _DEGTORAD(kViewer.fieldofview);
            m_kCurrentBuffer.m_nCameraFieldOfView.x = tan(nFieldOfView * 0.5f);
            m_kCurrentBuffer.m_nCameraFieldOfView.y = (m_kCurrentBuffer.m_nCameraFieldOfView.x / nAspect);
            m_kCurrentBuffer.m_nCameraMinDistance = kViewer.mindistance;
            m_kCurrentBuffer.m_nCameraMaxDistance = kViewer.maxdistance;

            m_kCurrentBuffer.m_nViewMatrix = nCameraMatrix.GetInverse();
            m_kCurrentBuffer.m_nProjectionMatrix.SetPerspective(nFieldOfView, nAspect, kViewer.mindistance, kViewer.maxdistance);
            m_kCurrentBuffer.m_nInvProjectionMatrix = m_kCurrentBuffer.m_nProjectionMatrix.GetInverse();
            m_kCurrentBuffer.m_nTransformMatrix = (m_kCurrentBuffer.m_nViewMatrix * m_kCurrentBuffer.m_nProjectionMatrix);

            m_nCurrentFrustum.Create(nCameraMatrix, m_kCurrentBuffer.m_nProjectionMatrix);

            LIGHTBUFFER kData;
            m_aVisibleLights.clear();
            m_pSceneManager->ListComponentsEntities({ L"transform", L"light" }, [&](const GEKENTITYID &nEntityID)->void
            {
                auto &kLight = m_pSceneManager->GetComponent<GET_COMPONENT_DATA(light)>(nEntityID, L"light");
                auto &kTransform = m_pSceneManager->GetComponent<GET_COMPONENT_DATA(transform)>(nEntityID, L"transform");
                if (m_nCurrentFrustum.IsVisible(sphere(kTransform.position, kLight.range)))
                {
                    kData.m_nPosition = (m_kCurrentBuffer.m_nViewMatrix * float4(kTransform.position, 1.0f));

                    kData.m_nInvRange = (1.0f / (kData.m_nRange = kLight.range));

                    kData.m_nColor = kLight.color;

                    m_aVisibleLights.push_back(kData);
                }
            });

            CGEKObservable::SendEvent(TGEKEvent<IGEKRenderObserver>(std::bind(&IGEKRenderObserver::OnCullScene, std::placeholders::_1)));

            m_spEngineBuffer->Update((void *)&m_kCurrentBuffer);
            spContext->GetGeometrySystem()->SetConstantBuffer(0, m_spEngineBuffer);

            spContext->GetVertexSystem()->SetConstantBuffer(0, m_spEngineBuffer);
            spContext->GetVertexSystem()->SetConstantBuffer(1, m_spOrthoBuffer);

            spContext->GetComputeSystem()->SetConstantBuffer(0, m_spEngineBuffer);
            spContext->GetComputeSystem()->SetConstantBuffer(1, m_spLightCountBuffer);

            spContext->GetPixelSystem()->SetConstantBuffer(0, m_spEngineBuffer);
            spContext->GetPixelSystem()->SetConstantBuffer(1, m_spMaterialBuffer);
            spContext->GetPixelSystem()->SetConstantBuffer(2, m_spLightCountBuffer);

            m_pCurrentPass = &m_aPasses[kViewer.pass];
            m_pCurrentPass->m_nCurrentBuffer = 0;

            for (auto &pFilter : m_pCurrentPass->m_aFilters)
            {
                m_pCurrentFilter = pFilter;
                pFilter->Draw(spContext);
                m_pCurrentFilter = nullptr;
            }

            CGEKObservable::SendEvent(TGEKEvent<IGEKRenderObserver>(std::bind(&IGEKRenderObserver::OnPostRender, std::placeholders::_1)));

            GEK3DVIDEO::VIEWPORT kViewport;
            kViewport.m_nTopLeftX = (kViewer.position.x * m_pSystem->GetXSize());
            kViewport.m_nTopLeftY = (kViewer.position.y * m_pSystem->GetYSize());
            kViewport.m_nXSize = (kViewer.size.x * m_pSystem->GetXSize());
            kViewport.m_nYSize = (kViewer.size.y * m_pSystem->GetYSize());
            kViewport.m_nMinDepth = 0.0f;
            kViewport.m_nMaxDepth = 1.0f;

            m_pVideoSystem->SetDefaultTargets(spContext);
            spContext->SetViewports({ kViewport });

            spContext->SetRenderStates(m_spRenderStates);
            spContext->SetBlendStates(float4(1.0f), 0xFFFFFFFF, m_spBlendStates);
            spContext->SetDepthStates(0x0, m_spDepthStates);
            spContext->GetComputeSystem()->SetProgram(nullptr);
            spContext->GetVertexSystem()->SetProgram(m_spVertexProgram);
            spContext->GetVertexSystem()->SetConstantBuffer(1, m_spOrthoBuffer);
            spContext->GetGeometrySystem()->SetProgram(nullptr);
            spContext->GetPixelSystem()->SetProgram(m_spPixelProgram);
            spContext->GetPixelSystem()->SetResource(0, m_pCurrentPass->m_aBuffers[m_pCurrentPass->m_nCurrentBuffer]);
            SetResource(spContext->GetPixelSystem(), 1, nullptr);
            spContext->SetVertexBuffer(0, 0, m_spVertexBuffer);
            spContext->SetIndexBuffer(0, m_spIndexBuffer);
            spContext->SetPrimitiveType(GEK3DVIDEO::PRIMITIVE::TRIANGLELIST);
            spContext->DrawIndexedPrimitive(6, 0, 0);

            m_pCurrentPass = nullptr;
        }
    });

    spContext->ClearResources();
    CComQIPtr<IGEK2DVideoSystem> sp2DVideoSystem(m_pVideoSystem);
    if (sp2DVideoSystem)
    {
        sp2DVideoSystem->Begin();

        CComPtr<IUnknown> spGray;
        sp2DVideoSystem->CreateBrush(float4(1.0f, 1.0f, 1.0f, 0.75f), &spGray);

        CComPtr<IUnknown> spFont;
        sp2DVideoSystem->CreateFont(L"Arial", 400, GEK2DVIDEO::FONT::NORMAL, 25.0f, &spFont);

        static DWORD nLastTime = 0;
        static std::list<UINT32> aFPS;
        static UINT32 nNumFrames = 0;
        static UINT32 nAverageFPS = 0;

        nNumFrames++;
        DWORD nCurrentTime = GetTickCount();
        if (nCurrentTime - nLastTime > 1000)
        {
            nLastTime = nCurrentTime;
            aFPS.push_back(nNumFrames);
            nNumFrames = 0;

            if (aFPS.size() > 10)
            {
                aFPS.pop_front();
            }

            nAverageFPS = 0;
            for (auto nFPS : aFPS)
            {
                nAverageFPS += nFPS;
            }

            nAverageFPS /= aFPS.size();
        }

        sp2DVideoSystem->DrawText({ 25.0f, 25.0f, 225.0f, 50.0f }, spFont, spGray, L"FPS: %d", nAverageFPS);

        sp2DVideoSystem->End();
    }

    m_pVideoSystem->Present(true);

    while (!m_pVideoSystem->IsEventSet(m_spFrameEvent))
    {
        Sleep(0);
    };

    m_pVideoSystem->SetEvent(m_spFrameEvent);
}

STDMETHODIMP_(const frustum &) CGEKRenderSystem::GetFrustum(void) const
{
    return m_nCurrentFrustum;
}
