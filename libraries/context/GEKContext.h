#pragma once

// Interfaces
#include "Public\IGEKObservable.h"
#include "Public\IGEKUnknown.h"
#include "Public\IGEKContext.h"
#include <atlpath.h>
#include <algorithm>
#include <list>
#include <map>

class CGEKUnknown : public IGEKUnknown
{
private:
    ULONG m_nRefCount;
    IGEKContext *m_pContext;

public:
    CGEKUnknown(IGEKContext *pContext = nullptr)
        : m_nRefCount(0)
        , m_pContext(pContext)
    {
    }

    virtual ~CGEKUnknown(void)
    {
    }

    STDMETHOD_(ULONG, AddRef)                   (THIS)
    {
        return InterlockedIncrement(&m_nRefCount);
    }

    STDMETHOD_(ULONG, Release)                  (THIS)
    {
        LONG nRefCount = InterlockedDecrement(&m_nRefCount);
        if (nRefCount == 0)
        {
            Destroy();
            delete this;
        }

        return nRefCount;
    }

    STDMETHOD(QueryInterface)                   (THIS_ REFIID rIID, LPVOID FAR *ppObject)
    {
        REQUIRE_RETURN(ppObject, E_INVALIDARG);

        HRESULT hRetVal = E_INVALIDARG;
        if (IsEqualIID(IID_IUnknown, rIID))
        {
            AddRef();
            (*ppObject) = dynamic_cast<IUnknown *>(this);
            _ASSERTE(*ppObject);
            hRetVal = S_OK;
        }
        else if (IsEqualIID(__uuidof(IGEKUnknown), rIID))
        {
            AddRef();
            (*ppObject) = dynamic_cast<IGEKUnknown *>(this);
            _ASSERTE(*ppObject);
            hRetVal = S_OK;
        }

        return hRetVal;
    }

    STDMETHOD(RegisterContext)                  (THIS_ IGEKContext *pContext)
    {
        REQUIRE_RETURN(pContext, E_INVALIDARG);
        m_pContext = pContext;
        return Initialize();
    }

    STDMETHOD(Initialize)                       (THIS)
    {
        return S_OK;
    }

    STDMETHOD_(void, Destroy)                   (THIS)
    {
    }

    STDMETHOD_(IGEKContext *, GetContext)       (THIS)
    {
        REQUIRE_RETURN(m_pContext, nullptr);
        return m_pContext;
    }

    STDMETHOD_(const IGEKContext *, GetContext) (THIS) const
    {
        REQUIRE_RETURN(m_pContext, nullptr);
        return m_pContext;
    }

    STDMETHOD_(IUnknown *, GetUnknown)          (THIS)
    {
        return dynamic_cast<IUnknown *>(this);
    }

    STDMETHOD_(const IUnknown *, GetUnknown)    (THIS) const
    {
        return dynamic_cast<const IUnknown *>(this);
    }
};

template <typename RESULT>
class CGEKEvent
{
public:
    virtual ~CGEKEvent(void)
    {
    }

    virtual RESULT operator () (IGEKObserver *pObserver) const = 0;
};

template <typename INTERFACE>
class TGEKEvent : public CGEKEvent<void>
{
private:
    std::function<void(INTERFACE *)> OnEvent;

public:
    TGEKEvent(std::function<void(INTERFACE *)> const &DoOnEvent) :
        OnEvent(DoOnEvent)
    {
    }

    virtual void operator () (IGEKObserver *pObserver) const
    {
        CComQIPtr<INTERFACE> spObserver(pObserver);
        if (spObserver)
        {
            OnEvent(spObserver);
        }
    }
};

template <typename INTERFACE>
class TGEKCheck : public CGEKEvent<HRESULT>
{
private:
    std::function<HRESULT(INTERFACE *)> OnEvent;

public:
    TGEKCheck(std::function<HRESULT(INTERFACE *)> const &DoOnEvent) :
        OnEvent(DoOnEvent)
    {
    }

    virtual HRESULT operator () (IGEKObserver *pObserver) const
    {
        CComQIPtr<INTERFACE> spObserver(pObserver);
        if (spObserver)
        {
            return OnEvent(spObserver);
        }

        return E_FAIL;
    }
};

class CGEKObservable : public IGEKObservable
{
private:
    std::list<IGEKObserver *> m_aObservers;

public:
    CGEKObservable(void)
    {
    }

    virtual ~CGEKObservable(void)
    {
    }

    STDMETHOD(AddObserver)      (THIS_ IGEKObserver *pObserver)
    {
        auto pIterator = std::find(m_aObservers.begin(), m_aObservers.end(), pObserver);
        if (pIterator == m_aObservers.end())
        {
            m_aObservers.push_back(pObserver);
            return S_OK;
        }

        return E_FAIL;
    }

    STDMETHOD(RemoveObserver)   (THIS_ IGEKObserver *pObserver)
    {
        auto pIterator = std::find(m_aObservers.begin(), m_aObservers.end(), pObserver);
        if (pIterator != m_aObservers.end())
        {
            m_aObservers.erase(pIterator);
            return S_OK;
        }

        return E_FAIL;
    }

    void SendEvent(const CGEKEvent<void> &kEvent)
    {
        for (auto &pObserver : m_aObservers)
        {
            kEvent(pObserver);
        }
    }

    HRESULT CheckEvent(const CGEKEvent<HRESULT> &kEvent)
    {
        HRESULT hRetVal = S_OK;
        for (auto &pObserver : m_aObservers)
        {
            hRetVal = kEvent(pObserver);
            if (FAILED(hRetVal))
            {
                break;
            }
        }

        return hRetVal;
    }

    static HRESULT AddObserver(IUnknown *pUnknown, IGEKObserver *pObserver)
    {
        IGEKObservable *pObservable = dynamic_cast<IGEKObservable *>(pUnknown);
        if (pObservable)
        {
            return pObservable->AddObserver(pObserver);
        }

        return E_FAIL;
    }

    static HRESULT RemoveObserver(IUnknown *pUnknown, IGEKObserver *pObserver)
    {
        IGEKObservable *pObservable = dynamic_cast<IGEKObservable *>(pUnknown);
        if (pObservable)
        {
            return pObservable->RemoveObserver(pObserver);
        }

        return E_FAIL;
    }
};

#define DECLARE_UNKNOWN(CLASS)                                                      \
    public:                                                                         \
        STDMETHOD(QueryInterface)(THIS_ REFIID rIID, void** ppObject);              \
        STDMETHOD_(ULONG, AddRef)(THIS);                                            \
        STDMETHOD_(ULONG, Release)(THIS);                                           \
    public:

#define BEGIN_INTERFACE_LIST(CLASS)                                                 \
    STDMETHODIMP_(ULONG) CLASS::AddRef(THIS)                                        \
    {                                                                               \
        return CGEKUnknown::AddRef();                                               \
    }                                                                               \
                                                                                    \
    STDMETHODIMP_(ULONG) CLASS::Release(THIS)                                       \
    {                                                                               \
        return CGEKUnknown::Release();                                              \
    }                                                                               \
                                                                                    \
    STDMETHODIMP CLASS::QueryInterface(THIS_ REFIID rIID, LPVOID FAR *ppObject)     \
    {                                                                               \
        REQUIRE_RETURN(ppObject, E_INVALIDARG);

#define INTERFACE_LIST_ENTRY(INTERFACE_IID, INTERFACE_CLASS)                        \
        if (IsEqualIID(INTERFACE_IID, rIID))                                        \
        {                                                                           \
            AddRef();                                                               \
            (*ppObject) = dynamic_cast<INTERFACE_CLASS *>(this);                    \
            _ASSERTE(*ppObject);                                                    \
            return S_OK;                                                            \
        }

#define INTERFACE_LIST_ENTRY_COM(INTERFACE_CLASS)                                   \
        if (IsEqualIID(__uuidof(INTERFACE_CLASS), rIID))                            \
        {                                                                           \
            AddRef();                                                               \
            (*ppObject) = dynamic_cast<INTERFACE_CLASS *>(this);                    \
            _ASSERTE(*ppObject);                                                    \
            return S_OK;                                                            \
        }

#define INTERFACE_LIST_ENTRY_MEMBER(INTERFACE_IID, OBJECT)                          \
        if ((OBJECT) && IsEqualIID(INTERFACE_IID, rIID))                            \
        {                                                                           \
            return (OBJECT)->QueryInterface(rIID, ppObject);                        \
        }

#define INTERFACE_LIST_ENTRY_MEMBER_COM(INTERFACE_CLASS, OBJECT)                    \
        if (IsEqualIID(__uuidof(INTERFACE_CLASS), rIID))                            \
        {                                                                           \
            return (OBJECT)->QueryInterface(__uuidof(INTERFACE_CLASS), ppObject);   \
        }

#define INTERFACE_LIST_ENTRY_DELEGATE(INTERFACE_IID, FUNCTION)                      \
        if (IsEqualIID(INTERFACE_IID, rIID))                                        \
        {                                                                           \
            return FUNCTION(rIID, ppObject);                                        \
        }

#define INTERFACE_LIST_ENTRY_DELEGATE_COM(INTERFACE_CLASS, FUNCTION)                \
        if (IsEqualIID(__uuidof(INTERFACE_CLASS), rIID))                            \
        {                                                                           \
            return FUNCTION(rIID, ppObject);                                        \
        }

#define INTERFACE_LIST_ENTRY_BASE(BASE_CLASS)                                       \
        if (SUCCEEDED(BASE_CLASS::QueryInterface(rIID, ppObject)))                  \
        {                                                                           \
            return S_OK;                                                            \
        }

#define END_INTERFACE_LIST                                                          \
        (*ppObject) = nullptr;                                                      \
        return E_INVALIDARG;                                                        \
    }

#define END_INTERFACE_LIST_UNKNOWN                                                  \
        return CGEKUnknown::QueryInterface(rIID, ppObject);                         \
    }

#define END_INTERFACE_LIST_BASE(BASE_CLASS)                                         \
        return BASE_CLASS::QueryInterface(rIID, ppObject);                          \
    }

#define END_INTERFACE_LIST_DELEGATE(FUNCTION)                                       \
        return FUNCTION(rIID, ppObject);                                            \
    }

#define REGISTER_CLASS(CLASS)                                                       \
HRESULT GEKCreateInstanceOf##CLASS##(IGEKUnknown **ppObject)                        \
{                                                                                   \
    REQUIRE_RETURN(ppObject, E_INVALIDARG);                                         \
                                                                                    \
    HRESULT hRetVal = E_OUTOFMEMORY;                                                \
    CComPtr<CLASS> spObject(new CLASS());                                           \
    if (spObject != nullptr)                                                        \
    {                                                                               \
        hRetVal = spObject->QueryInterface(IID_PPV_ARGS(ppObject));                 \
    }                                                                               \
                                                                                    \
    return hRetVal;                                                                 \
}

#define DECLARE_REGISTERED_CLASS(CLASS)                                             \
extern HRESULT GEKCreateInstanceOf##CLASS##(IGEKUnknown **ppObject);

#define DECLARE_CONTEXT_SOURCE(SOURCENAME)                                          \
extern "C" __declspec(dllexport)                                                    \
HRESULT GEKGetModuleClasses(                                                        \
    std::unordered_map<CLSID, std::function<HRESULT (IGEKUnknown **)>> &aClasses,   \
    std::unordered_map<CStringW, CLSID> &aNamedClasses,                             \
    std::unordered_map<CLSID, std::vector<CLSID>> &aTypedClasses)                   \
{                                                                                   \
    CLSID kLastCLSID = GUID_NULL;

#define ADD_CONTEXT_CLASS(CLASSID, CLASS)                                           \
    if (aClasses.find(CLASSID) == aClasses.end())                                   \
    {                                                                               \
        aClasses[CLASSID] = GEKCreateInstanceOf##CLASS##;                           \
        kLastCLSID = CLASSID;                                                       \
    }                                                                               \
    else                                                                            \
    {                                                                               \
        _ASSERTE(!"Duplicate class found in module: " #CLASSID);                    \
    }

#define ADD_CLASS_NAME(NAME)                                                        \
    if (aNamedClasses.find(NAME) == aNamedClasses.end())                            \
    {                                                                               \
        aNamedClasses[NAME] = kLastCLSID;                                           \
    }                                                                               \
    else                                                                            \
    {                                                                               \
        _ASSERTE(!"Duplicate name found in module: " #NAME);                        \
    }

#define ADD_CLASS_TYPE(TYPEID)                                                      \
    aTypedClasses[TYPEID].push_back(kLastCLSID);

#define END_CONTEXT_SOURCE                                                          \
    return S_OK;                                                                    \
}

class CGEKPerformance
{
private:
    IGEKContext *m_pContext;
    double m_nStartTime;
    CStringA m_strFile;
    UINT32 m_nLine;
    CStringA m_strFunction;
    CStringW m_strMessage;

public:
    CGEKPerformance(IGEKContext *pContext, LPCSTR pFile, UINT32 nLine, LPCSTR pFunction, LPCWSTR pMessage, ...)
        : m_pContext(pContext)
        , m_nLine(nLine)
        , m_strFunction(pFunction)
    {
        if (m_pContext != nullptr)
        {
            CPathA kFile(pFile);
            kFile.StripPath();
            m_strFile = kFile.m_strPath;
            m_nStartTime = m_pContext->GetTime();

            if (pMessage != nullptr)
            {
                va_list pArgs;
                va_start(pArgs, pMessage);
                m_strMessage.FormatV(pMessage, pArgs);
                va_end(pArgs);
            }

            m_pContext->Log(m_strFile, m_nLine, GEK_LOGSTART, L"%20S: %s", m_strFunction.GetString(), m_strMessage.GetString());
        }
    }

    ~CGEKPerformance(void)
    {
        if (m_pContext != nullptr)
        {
            double nEndTime = m_pContext->GetTime();
            double nTime = (nEndTime - m_nStartTime);
            m_pContext->Log(m_strFile, m_nLine, GEK_LOGEND, L"%20S: %2.5fs", m_strFunction.GetString(), nTime);
        }
    }
};

#define GEKLOG(MESSAGE, ...)                    GetContext()->Log(__FILE__, __LINE__, GEK_LOG, MESSAGE, __VA_ARGS__)

#define GEKRESULT(RESULT, MESSAGE, ...)         if(!(RESULT)) { GetContext()->Log(__FILE__, __LINE__, GEK_LOG, MESSAGE, __VA_ARGS__); }

#define GEKFUNCTION(MESSAGE, ...)               CGEKPerformance kPerformanceTimer(GetContext(), __FILE__, __LINE__, __FUNCTION__, MESSAGE, __VA_ARGS__)

#define GEKSETMETRIC(NAME, VALUE)               GetContext()->SetMetric(NAME, VALUE)
#define GEKINCREMENTMETRIC(NAME)                GetContext()->IncrementMetric(NAME)
#define GEKLOGMETRICS(MESSAGE, ...)             GetContext()->LogMetrics(__FILE__, __LINE__, MESSAGE, __VA_ARGS__)
