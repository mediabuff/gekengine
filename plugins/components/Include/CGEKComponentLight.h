#pragma once

#include "GEKUtility.h"
#include "GEKContext.h"
#include "CGEKComponent.h"
#include "GEKAPI.h"
#include <list>

class CGEKComponentLight : public CGEKUnknown
                         , public CGEKComponent
{
public:
    float3 m_nColor;
    float m_nRange;

public:
    DECLARE_UNKNOWN(CGEKComponentLight)
    CGEKComponentLight(IGEKEntity *pEntity);
    ~CGEKComponentLight(void);

    // IGEKComponent
    STDMETHOD_(LPCWSTR, GetType)            (THIS) const;
    STDMETHOD_(void, ListProperties)        (THIS_ std::function<void(LPCWSTR, const GEKVALUE &)> OnProperty);
    STDMETHOD_(bool, GetProperty)           (THIS_ LPCWSTR pName, GEKVALUE &kValue) const;
    STDMETHOD_(bool, SetProperty)           (THIS_ LPCWSTR pName, const GEKVALUE &kValue);
};

class CGEKComponentSystemLight : public CGEKUnknown
                               , public CGEKContextUser
                               , public CGEKSceneManagerUser
                               , public CGEKViewManagerUser
                               , public IGEKComponentSystem
{
private:
    std::map<IGEKEntity *, CComPtr<CGEKComponentLight>> m_aComponents;

public:
    DECLARE_UNKNOWN(CGEKComponentSystemLight)
    CGEKComponentSystemLight(void);
    ~CGEKComponentSystemLight(void);

    // IGEKComponentSystem
    STDMETHOD_(void, Clear)             (THIS);
    STDMETHOD(Destroy)                  (THIS_ IGEKEntity *pEntity);
    STDMETHOD(Create)                   (THIS_ const CLibXMLNode &kEntityNode, IGEKEntity *pEntity, IGEKComponent **ppComponent);
    STDMETHOD_(void, GetVisible)        (THIS_ const frustum &kFrustum, concurrency::concurrent_unordered_set<IGEKEntity *> &aVisibleEntities);
};