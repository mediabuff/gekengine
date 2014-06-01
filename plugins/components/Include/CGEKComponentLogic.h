#pragma once

#include "GEKUtility.h"
#include "GEKContext.h"
#include "CGEKComponent.h"
#include "GEKAPI.h"
#include <concurrent_unordered_map.h>

class CGEKComponentLogic : public CGEKUnknown
                         , public CGEKComponent
{
public:
    CStringW m_strDefaultState;
    CComPtr<IGEKLogicState> m_spState;

public:
    DECLARE_UNKNOWN(CGEKComponentLogic)
    CGEKComponentLogic(IGEKContext *pContext, IGEKEntity *pEntity);
    ~CGEKComponentLogic(void);

    void SetState(IGEKLogicState *pState);

    // IGEKComponent
    STDMETHOD_(void, ListProperties)        (THIS_ std::function<void(LPCWSTR, const GEKVALUE &)> OnProperty);
    STDMETHOD_(bool, GetProperty)           (THIS_ LPCWSTR pName, GEKVALUE &kValue) const;
    STDMETHOD_(bool, SetProperty)           (THIS_ LPCWSTR pName, const GEKVALUE &kValue);
    STDMETHOD(OnEntityCreated)              (THIS);
    STDMETHOD_(void, OnEvent)               (THIS_ LPCWSTR pAction, const GEKVALUE &kParamA, const GEKVALUE &kParamB);
};

class CGEKComponentSystemLogic : public CGEKUnknown
                               , public IGEKSceneObserver
                               , public IGEKComponentSystem
                               , public IGEKLogicSystem
{
private:
    concurrency::concurrent_unordered_map<IGEKEntity *, CComPtr<CGEKComponentLogic>> m_aComponents;

public:
    DECLARE_UNKNOWN(CGEKComponentSystemLogic)
    CGEKComponentSystemLogic(void);
    ~CGEKComponentSystemLogic(void);

    // IGEKSceneObserver
    STDMETHOD_(void, OnPreUpdate)           (THIS_ float nGameTime, float nFrameTime);
    STDMETHOD_(void, OnFree)                (THIS);

    // IGEKUnknown
    STDMETHOD(Initialize)                   (THIS);
    STDMETHOD_(void, Destroy)               (THIS);

    // IGEKComponentSystem
    STDMETHOD_(LPCWSTR, GetType)            (THIS) const;
    STDMETHOD(Destroy)                      (THIS_ IGEKEntity *pEntity);
    STDMETHOD(Create)                       (THIS_ const CLibXMLNode &kComponentNode, IGEKEntity *pEntity, IGEKComponent **ppComponent);

    // IGEKLogicSystem
    STDMETHOD_(void, SetState)              (IGEKEntity *pEntity, IGEKLogicState *pState);
};