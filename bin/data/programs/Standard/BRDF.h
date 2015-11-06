// Diffuse & Specular Term
// http://www.gamedev.net/topic/639226-your-preferred-or-desired-brdf/
void getBRDF(in float3 albedoTerm, in float3 pixelNormal, in float3 lightNormal, in float3 viewNormal, in float4 pixelInfo, out float3 diffuseContribution, out float3 specularContribution)
{
    float materialRoughness = clamp(pixelInfo.x, 0.1, 1.0);
    float materialRoughnessSquared = (materialRoughness * materialRoughness);
    float materialSpecular = pixelInfo.y;
    float materialMetalness = pixelInfo.z;

    float3 Ks = lerp(materialSpecular, albedoTerm, materialMetalness);
    float3 Kd = lerp(albedoTerm, 0, materialMetalness);
    float3 Fd = 1.0 - Ks;

    float angleCenterView = dot(pixelNormal, viewNormal);
    float angleCenterViewSquared = (angleCenterView * angleCenterView);
    float inverseAngleCenterViewSquared = (1.0 - angleCenterViewSquared);

    float3 halfVector = normalize(lightNormal + viewNormal);
    float angleCenterHalf = saturate(dot(pixelNormal, halfVector));
    float angleLightHalf = saturate(dot(lightNormal, halfVector));

    float angleCenterLight = saturate(dot(pixelNormal, lightNormal));
    float angleCenterLightSquared = (angleCenterLight * angleCenterLight);
    float angleCenterHalfSquared = (angleCenterHalf * angleCenterHalf);
    float inverseAngleCenterLightSquared = (1.0 - angleCenterLightSquared);

    diffuseContribution = (Kd * Fd * Math::ReciprocalPi * saturate(((1 - materialRoughness) * 0.5) + 0.5 + (materialRoughnessSquared * (8 - materialRoughness) * 0.023)));

    float centeredAngleCenterView = ((angleCenterView * 0.5) + 0.5);
    float centeredAngleCenterLight = ((angleCenterLight * 0.5) + 0.5);
    angleCenterViewSquared = (centeredAngleCenterView * centeredAngleCenterView);
    angleCenterLightSquared = (centeredAngleCenterLight * centeredAngleCenterLight);
    inverseAngleCenterViewSquared = (1.0 - angleCenterViewSquared);
    inverseAngleCenterLightSquared = (1.0 - angleCenterLightSquared);

    float diffuseDelta = lerp((1 / (0.1 + materialRoughness)), (-materialRoughnessSquared * 2), saturate(materialRoughness));
    float diffuseViewAngle = (1 - (pow(1 - centeredAngleCenterView, 4) * diffuseDelta));
    float diffuseLightAngle = (1 - (pow(1 - centeredAngleCenterLight, 4) * diffuseDelta));
    diffuseContribution *= (diffuseLightAngle * diffuseViewAngle * angleCenterLight);

    float3 Fs = (Ks + (Fd * pow((1 - angleLightHalf), 5)));
    float D = (pow(materialRoughness / (angleCenterHalfSquared * (materialRoughnessSquared + (1 - angleCenterHalfSquared) / angleCenterHalfSquared)), 2) * Math::ReciprocalPi);
    specularContribution = (Fs * D * angleCenterLight);
}