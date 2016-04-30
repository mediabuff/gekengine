#include "GEKEngine"

#include "GEKGlobal.hlsl"
#include "GEKUtility.hlsl"

#include "BRDF.Custom.h"

float3 mainPixelProgram(InputPixel inputPixel) : SV_TARGET0
{
    float3 materialAlbedo = Resources::albedoBuffer.Sample(Global::pointSampler, inputPixel.texCoord);
    float2 materialInfo = Resources::materialBuffer.Sample(Global::pointSampler, inputPixel.texCoord);
    float materialRoughness = ((materialInfo.x * 0.9) + 0.1); // account for infinitely small point lights
    float materialMetalness = materialInfo.y;

    float surfaceDepth = Resources::depthBuffer.Sample(Global::pointSampler, inputPixel.texCoord);
    float3 surfacePosition = getViewPosition(inputPixel.texCoord, surfaceDepth);
    float3 surfaceNormal = decodeNormal(Resources::normalBuffer.Sample(Global::pointSampler, inputPixel.texCoord));

    float3 viewDirection = -normalize(surfacePosition);
    float3 reflectNormal = reflect(-viewDirection, surfaceNormal);

    const uint2 tilePosition = uint2(floor(inputPixel.position.xy / float(tileSize).xx));
    const uint tileIndex = ((tilePosition.y * dispatchWidth) + tilePosition.x);
    const uint bufferOffset = (tileIndex * (Lighting::lightsPerPass + 1));
    uint lightTileCount = Resources::tileIndexList[bufferOffset];
    uint lightTileStart = (bufferOffset + 1);
    uint lightTileEnd = (lightTileStart + lightTileCount);

    float3 surfaceColor = 0.0;

    [loop]
    for (uint lightTileIndex = lightTileStart; lightTileIndex < lightTileEnd; ++lightTileIndex)
    {
        uint lightIndex = Resources::tileIndexList[lightTileIndex];
        Lighting::Data light = Lighting::list[lightIndex];

        float lightFalloff = 0.0;
        float3 lightDirection = 0.0;
        if (light.type == Lighting::Type::Point)
        {
            float3 lightRay = (light.position.xyz - surfacePosition);
            float3 centerToRay = ((dot(lightRay, reflectNormal) * reflectNormal) - lightRay);
            float3 closestPoint = (lightRay + (centerToRay * clamp((light.radius / length(centerToRay)), 0.0, 1.0)));
            lightDirection = normalize(closestPoint);
            float lightDistance = length(closestPoint);

            float distanceOverRange = (lightDistance / light.range);
            float distanceOverRange2 = sqr(distanceOverRange);
            float distanceOverRange4 = sqr(distanceOverRange2);
            lightFalloff = sqr(saturate(1.0 - distanceOverRange4));
            lightFalloff /= (sqr(lightDistance) + 1.0);
        }
        else if (light.type == Lighting::Type::Spot)
        {
        }
        else if (light.type == Lighting::Type::Directional)
        {
            lightDirection = light.direction;
            lightFalloff = 1.0;
        }

        float NdotL = dot(surfaceNormal, lightDirection);
        float3 diffuseAlbedo = lerp(materialAlbedo, 0.0, materialMetalness);
        float3 diffuseLighting = diffuseAlbedo;//(diffuseAlbedo * Math::ReciprocalPi);
        float3 specularLighting = getSpecularBRDF(materialAlbedo, materialRoughness, materialMetalness, surfaceNormal, lightDirection, viewDirection, NdotL);
        surfaceColor += (saturate(NdotL) * (diffuseLighting + specularLighting) * lightFalloff * light.color);
    }

    return surfaceColor;
}
