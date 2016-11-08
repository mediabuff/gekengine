/// @file
/// @author Todd Zupan <toddzupan@gmail.com>
/// @version $Revision: 68c94ed58445f7f7b11fb87263c60bc483158d4d $
/// @section LICENSE
/// https://en.wikipedia.org/wiki/MIT_License
/// @section DESCRIPTION
/// Last Changed: $Date:   Fri Oct 21 13:54:12 2016 +0000 $
#pragma once

#include "GEK\Math\Vector3.hpp"
#include "GEK\Math\Matrix4x4SIMD.hpp"
#include "GEK\Math\Quaternion.hpp"
#include "GEK\Math\Convert.hpp"
#include "GEK\Engine\Component.hpp"

namespace Gek
{
    namespace Components
    {
        GEK_COMPONENT(Transform)
        {
            void *operator new(size_t size)
            {
                return _mm_malloc(size * sizeof(Transform), 16);
            }

            void operator delete(void *data)
            {
                _mm_free(data);
            }

            Math::Float3 position = Math::Float3::Zero;
            Math::FloatQuat rotation = Math::FloatQuat::Identity;
            Math::Float3 scale = Math::Float3::One;

            void save(JSON::Object &componentData) const;
            void load(const JSON::Object &componentData);

            inline Math::SIMD::Float4x4 getMatrix(void) const
            {
                auto matrix(Math::convert(rotation));
                matrix.translation = position;
                return matrix;
            }
        };
    }; // namespace Components
}; // namespace Gek
