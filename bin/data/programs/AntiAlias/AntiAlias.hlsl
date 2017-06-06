#include GEKEngine

#include <GEKGlobal.hlsl>
#include <GEKUtility.hlsl>

namespace Defines
{
    static const float ReduceMinimum = 1.0 / 128.0;
    static const float ReduceMultiplier = 1.0 / 8.0;
    static const float SpanMaximum = 8.0;
}; // namespace Defines

// https://github.com/mattdesl/glsl-fxaa
//optimized version for mobile, where dependent 
//texture reads can be a bottleneck
float3 mainPixelProgram(InputPixel inputPixel) : SV_TARGET0
{
    const float3 colorMD = Resources::screenBuffer[inputPixel.screen.xy];
    const float3 colorNW = Resources::screenBuffer[inputPixel.screen.xy + float2(-1, +1)];
    const float3 colorNE = Resources::screenBuffer[inputPixel.screen.xy + float2(+1, +1)];
    const float3 colorSW = Resources::screenBuffer[inputPixel.screen.xy + float2(-1, -1)];
    const float3 colorSE = Resources::screenBuffer[inputPixel.screen.xy + float2(+1, -1)];

    const float luminanceMD = GetLuminance(colorMD);
    const float luminanceNW = GetLuminance(colorNW);
    const float luminanceNE = GetLuminance(colorNE);
    const float luminanceSW = GetLuminance(colorSW);
    const float luminanceSE = GetLuminance(colorSE);
    const float luminanceMinimum = min(luminanceMD, min(min(luminanceNW, luminanceNE), min(luminanceSW, luminanceSE)));
    const float luminanceMaximum = max(luminanceMD, max(max(luminanceNW, luminanceNE), max(luminanceSW, luminanceSE)));

    float2 direction;
    direction.x = -((luminanceNW + luminanceNE) - (luminanceSW + luminanceSE));
    direction.y = ((luminanceNW + luminanceSW) - (luminanceNE + luminanceSE));
    const float dirReduce = max((luminanceNW + luminanceNE + luminanceSW + luminanceSE) * (0.25 * Defines::ReduceMultiplier), Defines::ReduceMinimum);

    const float recipricalDirection = 1.0 / (min(abs(direction.x), abs(direction.y)) + dirReduce);
    direction = min(Defines::SpanMaximum, max(-Defines::SpanMaximum, direction * recipricalDirection)) * Shader::TargetPixelSize;

    float3 colorA = Resources::screenBuffer[inputPixel.screen.xy + direction * (1.0 / 3.0 - 0.5)];
    colorA += Resources::screenBuffer[inputPixel.screen.xy + direction * (2.0 / 3.0 - 0.5)];
    colorA *= 0.5;

    float3 colorB = Resources::screenBuffer[inputPixel.screen.xy + direction * -0.5];
    colorB += Resources::screenBuffer[inputPixel.screen.xy + direction * 0.5];
    colorB = ((colorA * 0.5) + (0.25 * colorB));

    const float luminanceB = GetLuminance(colorB);
    if ((luminanceB < luminanceMinimum) || (luminanceB > luminanceMaximum))
    {
        return colorA;
    }
    else
    {
        return colorB;
    }
}
