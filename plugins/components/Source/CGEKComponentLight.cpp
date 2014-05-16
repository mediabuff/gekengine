#include "CGEKComponentLight.h"
#include <algorithm>
#include <ppl.h>

BEGIN_INTERFACE_LIST(CGEKComponentLight)
    INTERFACE_LIST_ENTRY_COM(IGEKComponent)
END_INTERFACE_LIST_UNKNOWN

CGEKComponentLight::CGEKComponentLight(IGEKEntity *pEntity)
    : CGEKComponent(pEntity)
    , m_nRange(0.0f)
{
}

CGEKComponentLight::~CGEKComponentLight(void)
{
}

STDMETHODIMP_(LPCWSTR) CGEKComponentLight::GetType(void) const
{
    return L"light";
}

STDMETHODIMP_(void) CGEKComponentLight::ListProperties(std::function<void(LPCWSTR, const GEKVALUE &)> OnProperty)
{
    OnProperty(L"color", m_nColor);
    OnProperty(L"range", m_nRange);
}

static GEKHASH gs_nColor(L"color");
static GEKHASH gs_nRange(L"range");
STDMETHODIMP_(bool) CGEKComponentLight::GetProperty(LPCWSTR pName, GEKVALUE &kValue) const
{
    GEKHASH nHash(pName);
    if (nHash == gs_nColor)
    {
        kValue = m_nColor;
        return true;
    }
    else if (nHash == gs_nRange)
    {
        kValue = m_nRange;
        return true;
    }

    return false;
}

STDMETHODIMP_(bool) CGEKComponentLight::SetProperty(LPCWSTR pName, const GEKVALUE &kValue)
{
    GEKHASH nHash(pName);
    if (nHash == gs_nColor)
    {
        m_nColor = kValue.GetFloat3();
        return true;
    }
    else if (nHash == gs_nRange)
    {
        m_nRange = kValue.GetFloat();
        return true;
    }

    return false;
}

BEGIN_INTERFACE_LIST(CGEKComponentSystemLight)
    INTERFACE_LIST_ENTRY_COM(IGEKContextUser)
    INTERFACE_LIST_ENTRY_COM(IGEKSceneManagerUser)
    INTERFACE_LIST_ENTRY_COM(IGEKViewManagerUser)
    INTERFACE_LIST_ENTRY_COM(IGEKComponentSystem)
END_INTERFACE_LIST_UNKNOWN

REGISTER_CLASS(CGEKComponentSystemLight)

CGEKComponentSystemLight::CGEKComponentSystemLight(void)
{
}

CGEKComponentSystemLight::~CGEKComponentSystemLight(void)
{
}

STDMETHODIMP_(void) CGEKComponentSystemLight::Clear(void)
{
    m_aComponents.clear();
}

STDMETHODIMP CGEKComponentSystemLight::Create(const CLibXMLNode &kEntityNode, IGEKEntity *pEntity, IGEKComponent **ppComponent)
{
    HRESULT hRetVal = E_FAIL;
    if (kEntityNode.HasAttribute(L"type") && kEntityNode.GetAttribute(L"type").CompareNoCase(L"light") == 0)
    {
        hRetVal = E_OUTOFMEMORY;
        CComPtr<CGEKComponentLight> spComponent(new CGEKComponentLight(pEntity));
        if (spComponent)
        {
            CComPtr<IUnknown> spComponentUnknown;
            spComponent->QueryInterface(IID_PPV_ARGS(&spComponentUnknown));
            if (spComponentUnknown)
            {
                GetContext()->RegisterInstance(spComponentUnknown);
            }

            hRetVal = spComponent->QueryInterface(IID_PPV_ARGS(ppComponent));
            if (SUCCEEDED(hRetVal))
            {
                kEntityNode.ListAttributes([&spComponent] (LPCWSTR pName, LPCWSTR pValue) -> void
                {
                    spComponent->SetProperty(pName, pValue);
                } );

                m_aComponents[pEntity] = spComponent;
            }
        }
    }

    return hRetVal;
}

STDMETHODIMP CGEKComponentSystemLight::Destroy(IGEKEntity *pEntity)
{
    HRESULT hRetVal = E_FAIL;
    auto pIterator = std::find_if(m_aComponents.begin(), m_aComponents.end(), [&](std::map<IGEKEntity *, CComPtr<CGEKComponentLight>>::value_type &kPair) -> bool
    {
        return (kPair.first == pEntity);
    });

    if (pIterator != m_aComponents.end())
    {
        m_aComponents.erase(pIterator);
        hRetVal = S_OK;
    }

    return hRetVal;
}

STDMETHODIMP_(void) CGEKComponentSystemLight::GetVisible(const frustum &kFrustum, concurrency::concurrent_unordered_set<IGEKEntity *> &aVisibleEntities)
{
    concurrency::parallel_for_each(m_aComponents.begin(), m_aComponents.end(), [&](std::map<IGEKEntity *, CComPtr<CGEKComponentLight>>::value_type &kPair) -> void
    {
        if (kPair.second->m_nRange > 0.0f)
        {
            IGEKComponent *pTransform = kPair.first->GetComponent(L"transform");
            if (pTransform)
            {
                GEKVALUE kPosition;
                pTransform->GetProperty(L"position", kPosition);
                if (kFrustum.IsVisible(sphere(kPosition.GetFloat3(), kPair.second->m_nRange)))
                {
                    aVisibleEntities.insert(kPair.first);
                }
            }
        }
    });
}
