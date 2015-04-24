#include "CGEKComponentTransform.h"

REGISTER_COMPONENT(transform)
    REGISTER_COMPONENT_DEFAULT_VALUE(position, float3(0.0f, 0.0f, 0.0f))
    REGISTER_COMPONENT_DEFAULT_VALUE(rotation, quaternion(0.0f, 0.0f, 0.0f, 1.0f))
    REGISTER_COMPONENT_SERIALIZE(transform)
        REGISTER_COMPONENT_SERIALIZE_VALUE(position, StrFromFloat3)
        REGISTER_COMPONENT_SERIALIZE_VALUE(rotation, StrFromQuaternion)
    REGISTER_COMPONENT_DESERIALIZE(transform)
        REGISTER_COMPONENT_DESERIALIZE_VALUE(position, StrToFloat3)
        REGISTER_COMPONENT_DESERIALIZE_VALUE(rotation, StrToQuaternion)
END_REGISTER_COMPONENT(transform)