#include "Utility\Display.h"
#include <algorithm>

namespace Gek
{
    namespace Display
    {
        UINT8 getAspectRatio(UINT32 width, UINT32 height)
        {
            const float AspectRatio4x3 = (float(INT32((4.0f / 3.0f) * 100.0f)) / 100.0f);
            const float AspectRatio16x9 = (float(INT32((16.0f / 9.0f) * 100.0f)) / 100.0f);
            const float AspectRatio16x10 = (float(INT32((16.0f / 10.0f) * 100.0f)) / 100.0f);
            float aspectRatio = (float(INT32((float(width) / float(height)) * 100.0f)) / 100.0f);
            if (aspectRatio == AspectRatio4x3)
            {
                return AspectRatio::_4x3;
            }
            else if (aspectRatio == AspectRatio16x9)
            {
                return AspectRatio::_16x9;
            }
            else if (aspectRatio == AspectRatio16x10)
            {
                return AspectRatio::_16x10;
            }
            else
            {
                return AspectRatio::Unknown;
            }
        }

        std::map<UINT32, std::vector<Mode>> getModes(void)
        {
            UINT32 displayMode = 0;
            DEVMODE displayModeData = { 0 };
            std::map<UINT32, std::vector<Mode>> availableModes;
            while (EnumDisplaySettings(0, displayMode++, &displayModeData))
            {
                std::vector<Mode> &currentModes = availableModes[displayModeData.dmBitsPerPel];
                auto findIterator = std::find_if(currentModes.begin(), currentModes.end(), [&](const Mode &mode) -> bool
                {
                    if (mode.width != displayModeData.dmPelsWidth) return false;
                    if (mode.height != displayModeData.dmPelsHeight) return false;
                    return true;
                });

                if (findIterator == currentModes.end())
                {
                    currentModes.emplace_back(displayModeData.dmPanningWidth, displayModeData.dmPanningHeight,
                        getAspectRatio(displayModeData.dmPanningWidth, displayModeData.dmPanningHeight));
                }
            };

            return availableModes;
        }
    }; // namespace Display
}; // namespace Gek