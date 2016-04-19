﻿#include "GEK\Engine\Particles.h"
#include "GEK\Context\ContextUserMixin.h"
#include "GEK\Engine\ComponentMixin.h"
#include "GEK\Utility\String.h"

namespace Gek
{
    ParticlesComponent::ParticlesComponent(void)
    {
    }

    HRESULT ParticlesComponent::save(Population::ComponentDefinition &componentData) const
    {
        return S_OK;
    }

    HRESULT ParticlesComponent::load(const Population::ComponentDefinition &componentData)
    {
        return S_OK;
    }

    class ParticlesImplementation : public ContextUserMixin
        , public ComponentMixin<ParticlesComponent>
    {
    public:
        ParticlesImplementation(void)
        {
        }

        BEGIN_INTERFACE_LIST(ParticlesImplementation)
            INTERFACE_LIST_ENTRY_COM(Component)
        END_INTERFACE_LIST_USER

        // Component
        STDMETHODIMP_(LPCWSTR) getName(void) const
        {
            return L"particles";
        }
    };

    REGISTER_CLASS(ParticlesImplementation)
}; // namespace Gek