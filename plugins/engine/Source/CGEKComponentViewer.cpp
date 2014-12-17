#include "CGEKComponentViewer.h"

REGISTER_COMPONENT(viewer)
    REGISTER_SERIALIZE(fieldofview, StrFromFloat)
    REGISTER_SERIALIZE(minviewdistance, StrFromFloat)
    REGISTER_SERIALIZE(maxviewdistance, StrFromFloat)
    REGISTER_SERIALIZE(viewport, StrFromFloat4)
    REGISTER_SERIALIZE(pass, )
REGISTER_SEPARATOR(viewer)
    REGISTER_DESERIALIZE(fieldofview, StrToFloat)
    REGISTER_DESERIALIZE(minviewdistance, StrToFloat)
    REGISTER_DESERIALIZE(maxviewdistance, StrToFloat)
    REGISTER_DESERIALIZE(viewport, StrToFloat4)
    REGISTER_DESERIALIZE(pass, )
END_REGISTER_COMPONENT(viewer)
