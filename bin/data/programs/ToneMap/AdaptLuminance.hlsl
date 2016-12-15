#include GEKEngine

#include <GEKGlobal.hlsl>

namespace Defines
{
    static const float AdaptionRate = 1.25;
}; // namespace Defines

[numthreads(1, 1, 1)]
void mainComputeProgram(void)
{
    uint width, height, mipMapCount;
    Resources::luminanceBuffer.GetDimensions(0, width, height, mipMapCount);

    float averageLuminance = UnorderedAccess::averageLuminanceBuffer[0];
    const float currentLuminance = exp(Resources::luminanceBuffer.Load(uint3(0, 0, (mipMapCount - 1))));
    averageLuminance += (currentLuminance - averageLuminance) * (1.0 - exp(-Engine::FrameTime * Defines::AdaptionRate));
    averageLuminance = (isfinite(averageLuminance) ? averageLuminance : 0.0);
    UnorderedAccess::averageLuminanceBuffer[0] = max(averageLuminance, Math::Epsilon);
}
