#pragma once

#include "GEK\Math\Float3.h"
#include "GEK\Engine\Component.h"
#include <atlbase.h>
#include <atlstr.h>
#include <unordered_map>

namespace Gek
{
    struct CameraComponent
    {
        enum class Mode : UINT8
        {
            Perspective = 0,
            Orthographic,
        };

        Mode mode;
        float minimumDistance;
        float maximumDistance;
        union
        {
            float size; // Orthographic
            float fieldOfView; // Perspective
        };

        CameraComponent(void);
        void save(Population::ComponentDefinition &componentData) const;
        void load(const Population::ComponentDefinition &componentData);
    };

    struct FirstPersonCameraComponent
        : public CameraComponent
    {
        FirstPersonCameraComponent(void);
        void save(Population::ComponentDefinition &componentData) const;
        void load(const Population::ComponentDefinition &componentData);
    };

    struct ThirdPersonCameraComponent
        : public CameraComponent
    {
        String body;
        Math::Float3 offset;
        Math::Float3 distance;

        ThirdPersonCameraComponent(void);
        void save(Population::ComponentDefinition &componentData) const;
        void load(const Population::ComponentDefinition &componentData);
    };
}; // namespace Gek
