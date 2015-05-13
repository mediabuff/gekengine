#include "CGEKTimer.h"
#include <windows.h>

namespace Gek
{
    Timer::Timer(void)
        : pausedState(false)
    {
        reset();
    }

    void Timer::reset(void)
    {
        previousTime = currentTime = startTime = clock.now();
    }

    void Timer::update(void)
    {
        previousTime = currentTime;
        currentTime = clock.now();
    }

    void Timer::pause(bool state)
    {
        if (state && !pausedState)
        {
            pausedState = true;
            pausedTime = clock.now();
        }
        else if (!state && pausedState)
        {
            auto kTime = clock.now();
            auto kPaused = (kTime - pausedTime);
            startTime += kPaused;
            pausedState = false;
        }
    }

    double Timer::getUpdateTime(void) const
    {
        return (std::chrono::duration<double, std::milli>(currentTime - previousTime).count() * 0.001);
    }

    double Timer::getAbsoluteTime(void) const
    {
        return (std::chrono::duration<double, std::milli>(currentTime - startTime).count() * 0.001);
    }
}; // namespace Gek
