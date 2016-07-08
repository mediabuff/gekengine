#include "GEK\Context\ContextUser.h"

namespace Gek
{
    namespace DirectInput8
    {
        GEK_DECLARE_CONTEXT_USER(System);
    };

    namespace DirectSound8
    {
        GEK_DECLARE_CONTEXT_USER(Device);
    };

    namespace Direct3D11
    {
        GEK_DECLARE_CONTEXT_USER(Device);
    };

    GEK_CONTEXT_BEGIN(System);
        GEK_CONTEXT_ADD_CLASS(System::Input, DirectInput8::System);
        GEK_CONTEXT_ADD_CLASS(Device::Audio, DirectSound8::Device);
        GEK_CONTEXT_ADD_CLASS(Device::Video, Direct3D11::Device);
    GEK_CONTEXT_END();
}; // namespace Gek