/// @file
/// @author Todd Zupan <toddzupan@gmail.com>
/// @version $Revision$
/// @section LICENSE
/// https://en.wikipedia.org/wiki/MIT_License
/// @section DESCRIPTION
/// Last Changed: $Date$
#pragma once

#include "GEK\Math\Vector4.hpp"
#include "GEK\Math\Matrix4x4.hpp"
#include "GEK\Utility\Exceptions.hpp"
#include "GEK\Utility\Context.hpp"

namespace Gek
{
    namespace Audio
    {
        GEK_START_EXCEPTIONS();
        GEK_ADD_EXCEPTION(CreationFailed);
        GEK_ADD_EXCEPTION(InitailizeDeviceFailed);
        GEK_ADD_EXCEPTION(LoadFileFailed);
        GEK_ADD_EXCEPTION(CreateSampleFailed);
        GEK_ADD_EXCEPTION(WriteSampleFailed);

		GEK_INTERFACE(Buffer)
		{
		};

		GEK_INTERFACE(Sound)
		{
		};

        GEK_INTERFACE(Device)
        {
            virtual void setVolume(float volume) = 0;
            virtual float getVolume(void) = 0;

            virtual void setListener(const Math::Float4x4 &matrix) = 0;

			virtual BufferPtr loadBuffer(const wchar_t *fileName) = 0;
			virtual SoundPtr createSound(void) = 0;
        };
    }; // namespace Audio
}; // namespace Gek
