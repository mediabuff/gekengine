#pragma once

#include "GEK\Math\Vector4.h"
#include <atlbase.h>
#include <atlstr.h>
#include <unordered_map>

namespace Gek
{
    namespace Model
    {
        struct Data
        {
            CStringW value;

            Data(void);
            HRESULT getData(std::unordered_map<CStringW, CStringW> &componentParameterList) const;
            HRESULT setData(const std::unordered_map<CStringW, CStringW> &componentParameterList);

            inline operator LPCWSTR () const
            {
                return value.GetString();
            }

            inline operator CStringW () const
            {
                return value;
            }

            inline CStringW &operator = (LPCWSTR value)
            {
                this->value = value;
                return this->value;
            }
        };
    }; // namespace Model
}; // namespace Gek
