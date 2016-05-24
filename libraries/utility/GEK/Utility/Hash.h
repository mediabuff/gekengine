#pragma once

#include <Windows.h>
#include <algorithm>
#include <string>

namespace std
{
    template <>
    struct hash<const char *>
    {
        size_t operator()(const char *&value) const
        {
            return hash<string>()(string(value));
        }
    };

    template <>
    struct hash<const wchar_t *>
    {
        size_t operator()(const wchar_t *&value) const
        {
            return hash<wstring>()(wstring(value));
        }
    };

    inline size_t hash_combine(const size_t upper, const size_t lower)
    {
        return upper ^ (lower + 0x9e3779b9 + (upper << 6) + (upper >> 2));
    }

    inline size_t hash_combine(void)
    {
        return 0;
    }

    template <typename T, typename... Ts>
    size_t hash_combine(const T& t, const Ts&... ts)
    {
        size_t seed = hash<T>()(t);
        if (sizeof...(ts) == 0)
        {
            return seed;
        }

        size_t remainder = hash_combine(ts...);
        return hash_combine(seed, remainder);
    }
};
