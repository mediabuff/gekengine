#include "GEK\Components\Light.h"
#include "GEK\Context\ContextUserMixin.h"
#include "GEK\Engine\ComponentMixin.h"
#include "GEK\Utility\String.h"

namespace Gek
{
    LightComponent::LightComponent(void)
    {
    }

    HRESULT LightComponent::save(std::unordered_map<CStringW, CStringW> &componentParameterList) const
    {
        componentParameterList[L"range"] = String::from(range);
        return S_OK;
    }

    HRESULT LightComponent::load(const std::unordered_map<CStringW, CStringW> &componentParameterList)
    {
        setParameter(componentParameterList, L"range", range, String::to<float>);
        return S_OK;
    }

    class LightImplementation : public ContextUserMixin
        , public ComponentMixin<LightComponent>
    {
    public:
        LightImplementation(void)
        {
        }

        BEGIN_INTERFACE_LIST(LightImplementation)
            INTERFACE_LIST_ENTRY_COM(Component)
        END_INTERFACE_LIST_USER

        // Component
        STDMETHODIMP_(LPCWSTR) getName(void) const
        {
            return L"light";
        }
    };

    REGISTER_CLASS(LightImplementation)
}; // namespace Gek