float sqr(float x)
{
    return x * x;
}

float rand(float2 co)
{
    return 0.5 + (frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453)) * 0.5;
}

float getLuminance(float3 color)
{
    return max(dot(color, float3(0.299, 0.587, 0.114)), Math::Epsilon);
}

float3 getExposedColor(float3 color, float averageLuminance, out float exposure)
{
    static const float KeyValue = 0.2f;

    // Use geometric mean
    averageLuminance = max(averageLuminance, Math::Epsilon);
    float linearExposure = (KeyValue / averageLuminance);
    exposure = log2(max(linearExposure, Math::Epsilon));
    exposure -= toneMappingThreshold;
    return (exp2(exposure) * color);
}

// using stereograpgic projection
// http://aras-p.info/texts/CompactNormalStorage.html
half2 encodeNormal(float3 n)
{
    half scale = 1.7777;
    half2 enc = n.xy / (n.z + 1.0);
    enc /= scale;
    enc = enc * 0.5 + 0.5;
    return enc;
}

float3 decodeNormal(half2 enc)
{
    half scale = 1.7777;
    half3 nn = half3(enc * 2.0 * scale - scale, 1.0);
    half g = 2.0 / dot(nn.xyz, nn.xyz);
    half3 n;
    n.xy = g*nn.xy;
    n.z = g - 1.0;
    return n;
}

float3x3 getCoTangentFrame(float3 position, float3 normal, float2 texCoord)
{
    normal = normalize(normal);

    // get edge vectors of the pixel triangle
    float3 positionDDX = ddx(position);
    float3 positionDDY = ddy(position);
    float2 texCoordDDX = ddx(texCoord);
    float2 texCoordDDY = ddy(texCoord);

    // solve the linear system
    float3 perpendicularDX = cross(normal, positionDDX);
    float3 perpendicularDY = cross(positionDDY, normal);
    float3 tangent =   perpendicularDY * texCoordDDX.x + perpendicularDX * texCoordDDY.x;
    float3 biTangent = perpendicularDY * texCoordDDX.y + perpendicularDX * texCoordDDY.y;

    // construct a scale-invariant frame 
    float reciprocal = rsqrt(max(dot(tangent, tangent), dot(biTangent, biTangent)));
    return float3x3(normalize(tangent * reciprocal), normalize(-biTangent * reciprocal), normal);
}

float3 getViewPosition(float2 texCoord, float depth)
{
    float2 adjustedCoord = texCoord;
    adjustedCoord.y = (1.0 - adjustedCoord.y);
    adjustedCoord.xy = (adjustedCoord.xy * 2.0 - 1.0);
    return (float3((adjustedCoord * Camera::fieldOfView), 1.0) * depth * Camera::maximumDistance);
}
