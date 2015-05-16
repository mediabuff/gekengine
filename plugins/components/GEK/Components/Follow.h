#pragma once

#pragma once

#include "GEK\Math\Vector4.h"
#include "GEK\Math\Quaternion.h"
#include <atlbase.h>
#include <atlstr.h>
#include <unordered_map>

namespace Gek
{
    namespace Components
    {
        namespace Follow
        {
            static const void *Identifier = nullptr;
            struct Data
            {
                CStringW target;
                Gek::Math::Float3 offset;
                Gek::Math::Quaternion rotation;

                Data(void);
                HRESULT getData(std::unordered_map<CStringW, CStringW> &componentParameterList) const;
                HRESULT setData(const std::unordered_map<CStringW, CStringW> &componentParameterList);
            };
        }; // namespace Follow
    }; // namespace Components
}; // namespace Gek