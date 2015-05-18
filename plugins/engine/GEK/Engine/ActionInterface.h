#pragma once

#include "GEK\Context\ObserverInterface.h"

namespace Gek
{
    namespace Engine
    {
        namespace Action
        {
            DECLARE_INTERFACE_IID_(Observer, Gek::ObserverInterface, "B1358995-5C9A-4177-AD73-D7F2DB0FD90B")
            {
                STDMETHOD_(void, onState)           (THIS_ LPCWSTR name, bool state) { };
                STDMETHOD_(void, onValue)           (THIS_ LPCWSTR name, float value) { };
            };
        }; // namespace Action
    }; // namespace Engine
}; // namespace Gek
