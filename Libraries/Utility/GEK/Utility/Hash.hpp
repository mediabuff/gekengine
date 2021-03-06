/// @file
/// @author Todd Zupan <toddzupan@gmail.com>
/// @version $Revision$
/// @section LICENSE
/// https://en.wikipedia.org/wiki/MIT_License
/// @section DESCRIPTION
/// Last Changed: $Date$
#pragma once

#include <functional>
#include <string>

namespace Gek
{
    inline size_t CombineHashes(const size_t upper, const size_t lower)
    {
        return upper ^ (lower + 0x9e3779b9 + (upper << 6) + (upper >> 2));
    }

    inline size_t GetHash(void)
    {
        return 0;
    }

    template <typename TYPE, typename... PARAMETERS>
    size_t GetHash(const TYPE &value, const PARAMETERS&... arguments)
    {
        size_t seed = std::hash<TYPE>()(value);
        size_t remainder = GetHash(arguments...);
        return CombineHashes(seed, remainder);
    }
};
