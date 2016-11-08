﻿#include "GEK\Utility\String.hpp"
#include "GEK\Utility\FileSystem.hpp"
#include "GEK\Utility\JSON.hpp"
#include "GEK\Utility\ContextUser.hpp"
#include "GEK\Utility\ThreadPool.hpp"
#include "GEK\Engine\Core.hpp"
#include "GEK\Engine\Renderer.hpp"
#include "GEK\Engine\Resources.hpp"
#include "GEK\Engine\Visual.hpp"
#include "GEK\Engine\Shader.hpp"
#include "GEK\Engine\Filter.hpp"
#include "GEK\Engine\Material.hpp"
#include "GEK\Engine\Population.hpp"
#include "GEK\Engine\Entity.hpp"
#include "GEK\Engine\Component.hpp"
#include "GEK\Engine\ComponentMixin.hpp"
#include "GEK\Components\Transform.hpp"
#include "GEK\Components\Color.hpp"
#include "GEK\Components\Light.hpp"
#include "GEK\Shapes\Sphere.hpp"
#include <concurrent_unordered_set.h>
#include <concurrent_vector.h>
#include <algorithm>
#include <ppl.h>

namespace Gek
{
    namespace Implementation
    {
        GEK_CONTEXT_USER(Renderer, Video::Device *, Plugin::Population *, Engine::Resources *)
            , public Plugin::Renderer
        {
        public:
            static const uint32_t GridWidth = 16;
            static const uint32_t GridHeight = 8;
            static const uint32_t GridDepth = 24;
            static const uint32_t GridSize = (GridWidth * GridHeight * GridDepth);

        public:
            struct EngineConstantData
            {
                float worldTime;
                float frameTime;
                float buffer[2];
            };

            struct CameraConstantData
            {
                Math::Float2 fieldOfView;
                float nearClip;
                float farClip;
                Math::SIMD::Float4x4 viewMatrix;
                Math::SIMD::Float4x4 projectionMatrix;
            };

            struct LightConstantData
            {
                Math::UInt3 gridSize;
                uint32_t directionalLightCount;
                Math::UInt2 tileSize;
                uint32_t pointLightCount;
                uint32_t spotLightCount;
            };

            struct ClusterTile
            {
                concurrency::concurrent_vector<uint32_t> pointLightList;
                concurrency::concurrent_vector<uint32_t> spotLightList;
            };

            struct ClusterData
            {
                uint32_t indexOffset;
                uint32_t pointLightCount;
                uint32_t spotLightCount;
            };

            struct DrawCallValue
            {
                union
                {
                    uint32_t value;
                    struct
                    {
                        MaterialHandle material;
                        VisualHandle plugin;
                        ShaderHandle shader;
                    };
                };

                std::function<void(Video::Device::Context *)> onDraw;

                DrawCallValue(const DrawCallValue &drawCallValue)
                    : value(drawCallValue.value)
                    , onDraw(drawCallValue.onDraw)
                {
                }

                DrawCallValue(DrawCallValue &&drawCallValue)
                    : value(drawCallValue.value)
                    , onDraw(std::move(drawCallValue.onDraw))
                {
                }

                DrawCallValue(MaterialHandle material, VisualHandle plugin, ShaderHandle shader, std::function<void(Video::Device::Context *)> onDraw)
                    : material(material)
                    , plugin(plugin)
                    , shader(shader)
                    , onDraw(onDraw)
                {
                }

                void operator = (const DrawCallValue &drawCallValue)
                {
                    value = drawCallValue.value;
                    onDraw = drawCallValue.onDraw;
                }
            };

            using DrawCallList = concurrency::concurrent_vector<DrawCallValue>;

            struct DrawCallSet
            {
                Engine::Shader *shader = nullptr;
                DrawCallList::iterator begin;
                DrawCallList::iterator end;

                DrawCallSet(Engine::Shader *shader, DrawCallList::iterator begin, DrawCallList::iterator end)
                    : shader(shader)
                    , begin(begin)
                    , end(end)
                {
                }

                DrawCallSet(const DrawCallSet &drawCallSet)
                    : shader(drawCallSet.shader)
                    , begin(drawCallSet.begin)
                    , end(drawCallSet.end)
                {
                }

                DrawCallSet(DrawCallSet &&drawCallSet)
                    : shader(std::move(drawCallSet.shader))
                    , begin(std::move(drawCallSet.begin))
                    , end(std::move(drawCallSet.end))
                {
                }

                DrawCallSet &operator = (const DrawCallSet &drawCallSet)
                {
                    shader = drawCallSet.shader;
                    begin = drawCallSet.begin;
                    end = drawCallSet.end;
                    return (*this);
                }

                DrawCallSet &operator = (DrawCallSet &&drawCallSet)
                {
                    shader = std::move(drawCallSet.shader);
                    begin = std::move(drawCallSet.begin);
                    end = std::move(drawCallSet.end);
                    return (*this);
                }
            };

        private:
            Video::Device *videoDevice = nullptr;
            Plugin::Population *population = nullptr;
            Engine::Resources *resources = nullptr;

            Video::ObjectPtr pointSamplerState;
            Video::ObjectPtr linearClampSamplerState;
            Video::ObjectPtr linearWrapSamplerState;
            Video::BufferPtr engineConstantBuffer;
            Video::BufferPtr cameraConstantBuffer;

            Video::ObjectPtr deferredVertexProgram;
            Video::ObjectPtr deferredPixelProgram;
            Video::ObjectPtr blendState;
            Video::ObjectPtr renderState;
            Video::ObjectPtr depthState;

            ThreadPool threadPool;
            concurrency::concurrent_unordered_set<Plugin::Entity *> directionalLightEntities;
            concurrency::concurrent_unordered_set<Plugin::Entity *> pointLightEntities;
            concurrency::concurrent_unordered_set<Plugin::Entity *> spotLightEntities;

            concurrency::concurrent_vector<DirectionalLightData> directionalLightList;
            concurrency::concurrent_vector<PointLightData> pointLightList;
            concurrency::concurrent_vector<SpotLightData> spotLightList;
            ClusterTile clusterLightsList[GridSize];
            ClusterData clusterDataList[GridSize];
            std::vector<uint32_t> clusterIndexList;
            uint32_t lightIndexCount = 0;

            Video::BufferPtr lightConstantBuffer;
            Video::BufferPtr directionalLightDataBuffer;
            Video::BufferPtr pointLightDataBuffer;
            Video::BufferPtr spotLightDataBuffer;
            Video::BufferPtr clusterDataBuffer;
            Video::BufferPtr clusterIndexBuffer;

            DrawCallList drawCallList;

            const Shapes::Frustum viewFrustum;
            const Math::SIMD::Float4x4 viewMatrix;
            const Math::SIMD::Float4x4 projectionMatrix;
            const float nearClip = 0.0f;
            const float farClip = 0.0f;

        public:
            Renderer(Context *context, Video::Device *videoDevice, Plugin::Population *population, Engine::Resources *resources)
                : ContextRegistration(context)
                , videoDevice(videoDevice)
                , population(population)
                , resources(resources)
                , threadPool(3)
            {
                population->onLoadBegin.connect<Renderer, &Renderer::onLoadBegin>(this);
                population->onLoadSucceeded.connect<Renderer, &Renderer::onLoadSucceeded>(this);
                population->onEntityCreated.connect<Renderer, &Renderer::onEntityCreated>(this);
                population->onEntityDestroyed.connect<Renderer, &Renderer::onEntityDestroyed>(this);
                population->onComponentAdded.connect<Renderer, &Renderer::onComponentAdded>(this);
                population->onComponentRemoved.connect<Renderer, &Renderer::onComponentRemoved>(this);

                Video::SamplerStateInformation pointSamplerStateData;
                pointSamplerStateData.filterMode = Video::SamplerStateInformation::FilterMode::AllPoint;
                pointSamplerStateData.addressModeU = Video::SamplerStateInformation::AddressMode::Clamp;
                pointSamplerStateData.addressModeV = Video::SamplerStateInformation::AddressMode::Clamp;
                pointSamplerState = videoDevice->createSamplerState(pointSamplerStateData);

                Video::SamplerStateInformation linearClampSamplerStateData;
                linearClampSamplerStateData.maximumAnisotropy = 8;
                linearClampSamplerStateData.filterMode = Video::SamplerStateInformation::FilterMode::Anisotropic;
                linearClampSamplerStateData.addressModeU = Video::SamplerStateInformation::AddressMode::Clamp;
                linearClampSamplerStateData.addressModeV = Video::SamplerStateInformation::AddressMode::Clamp;
                linearClampSamplerState = videoDevice->createSamplerState(linearClampSamplerStateData);

                Video::SamplerStateInformation linearWrapSamplerStateData;
                linearWrapSamplerStateData.maximumAnisotropy = 8;
                linearWrapSamplerStateData.filterMode = Video::SamplerStateInformation::FilterMode::Anisotropic;
                linearWrapSamplerStateData.addressModeU = Video::SamplerStateInformation::AddressMode::Wrap;
                linearWrapSamplerStateData.addressModeV = Video::SamplerStateInformation::AddressMode::Wrap;
                linearWrapSamplerState = videoDevice->createSamplerState(linearWrapSamplerStateData);

                Video::UnifiedBlendStateInformation blendStateInformation;
                blendState = videoDevice->createBlendState(blendStateInformation);

                Video::RenderStateInformation renderStateInformation;
                renderState = videoDevice->createRenderState(renderStateInformation);

                Video::DepthStateInformation depthStateInformation;
                depthState = videoDevice->createDepthState(depthStateInformation);

                engineConstantBuffer = videoDevice->createBuffer(sizeof(EngineConstantData), 1, Video::BufferType::Constant, 0);
                engineConstantBuffer->setName(L"engineConstantBuffer");

                cameraConstantBuffer = videoDevice->createBuffer(sizeof(CameraConstantData), 1, Video::BufferType::Constant, 0);
                cameraConstantBuffer->setName(L"cameraConstantBuffer");

                lightConstantBuffer = videoDevice->createBuffer(sizeof(LightConstantData), 1, Video::BufferType::Constant, 0);
                lightConstantBuffer->setName(L"lightConstantBuffer");

                static const wchar_t program[] =
                    L"struct Output" \
                    L"{" \
                    L"    float4 screen : SV_POSITION;" \
                    L"    float2 texCoord : TEXCOORD0;" \
                    L"};" \
                    L"" \
                    L"Output mainVertexProgram(in uint vertexID : SV_VertexID)" \
                    L"{" \
                    L"    Output output;" \
                    L"    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);" \
                    L"    output.screen = float4(output.texCoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);" \
                    L"    return output;" \
                    L"}" \
                    L"" \
                    L"struct Input" \
                    L"{" \
                    L"    float4 screen : SV_POSITION;\r\n" \
                    L"    float2 texCoord : TEXCOORD0;" \
                    L"};" \
                    L"" \
                    L"Texture2D<float3> inputBuffer : register(t0);" \
                    L"float3 mainPixelProgram(in Input input) : SV_TARGET0" \
                    L"{" \
                    L"    return inputBuffer[input.screen.xy];" \
                    L"}";

                auto compiledVertexProgram = resources->compileProgram(Video::PipelineType::Vertex, L"deferredVertexProgram", L"mainVertexProgram", program);
                deferredVertexProgram = videoDevice->createProgram(Video::PipelineType::Vertex, compiledVertexProgram.data(), compiledVertexProgram.size());
                deferredVertexProgram->setName(L"deferredVertexProgram");

                auto compiledPixelProgram = resources->compileProgram(Video::PipelineType::Pixel, L"deferredPixelProgram", L"mainPixelProgram", program);
                deferredPixelProgram = videoDevice->createProgram(Video::PipelineType::Pixel, compiledPixelProgram.data(), compiledPixelProgram.size());
                deferredPixelProgram->setName(L"deferredPixelProgram");

                directionalLightList.reserve(10);
                directionalLightDataBuffer = videoDevice->createBuffer(sizeof(DirectionalLightData), directionalLightList.capacity(), Video::BufferType::Structured, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                directionalLightDataBuffer->setName(L"directionalLightDataBuffer");

                pointLightList.reserve(200);
                pointLightDataBuffer = videoDevice->createBuffer(sizeof(PointLightData), pointLightList.capacity(), Video::BufferType::Structured, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                pointLightDataBuffer->setName(L"pointLightDataBuffer");

                spotLightList.reserve(200);
                spotLightDataBuffer = videoDevice->createBuffer(sizeof(SpotLightData), spotLightList.capacity(), Video::BufferType::Structured, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                spotLightDataBuffer->setName(L"spotLightDataBuffer");

                clusterDataBuffer = videoDevice->createBuffer(Video::Format::R32G32B32_UINT, GridSize, Video::BufferType::Raw, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                clusterDataBuffer->setName(L"clusterDataBuffer");

                clusterIndexList.reserve(GridSize * 10);
                clusterIndexBuffer = videoDevice->createBuffer(Video::Format::R32_UINT, clusterIndexList.capacity(), Video::BufferType::Raw, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                clusterIndexBuffer->setName(L"clusterIndexBuffer");
            }

            ~Renderer(void)
            {
                population->onComponentRemoved.disconnect<Renderer, &Renderer::onComponentRemoved>(this);
                population->onComponentAdded.disconnect<Renderer, &Renderer::onComponentAdded>(this);
                population->onEntityDestroyed.disconnect<Renderer, &Renderer::onEntityDestroyed>(this);
                population->onEntityCreated.disconnect<Renderer, &Renderer::onEntityCreated>(this);
                population->onLoadSucceeded.disconnect<Renderer, &Renderer::onLoadSucceeded>(this);
                population->onLoadBegin.disconnect<Renderer, &Renderer::onLoadBegin>(this);
            }

            void addEntity(Plugin::Entity *entity)
            {
                if (entity->hasComponent<Components::Transform>())
                {
                    if (entity->hasComponent<Components::DirectionalLight>())
                    {
                        directionalLightEntities.insert(entity);
                    }

                    if (entity->hasComponent<Components::PointLight>())
                    {
                        pointLightEntities.insert(entity);
                    }

                    if (entity->hasComponent<Components::SpotLight>())
                    {
                        spotLightEntities.insert(entity);
                    }
                }
            }

            void removeEntity(Plugin::Entity *entity)
            {
                directionalLightEntities.unsafe_erase(entity);
                pointLightEntities.unsafe_erase(entity);
                spotLightEntities.unsafe_erase(entity);
            }

            // Clustered Lighting
            inline Math::Float3 getLightDirection(const Math::FloatQuat &quaternion)
            {
                float xx(quaternion.x * quaternion.x);
                float yy(quaternion.y * quaternion.y);
                float zz(quaternion.z * quaternion.z);
                float ww(quaternion.w * quaternion.w);
                float length(xx + yy + zz + ww);
                if (length == 0.0f)
                {
                    return Math::Float3(0.0f, 1.0f, 0.0f);
                }
                else
                {
                    float determinant(1.0f / length);
                    float xy(quaternion.x * quaternion.y);
                    float xw(quaternion.x * quaternion.w);
                    float yz(quaternion.y * quaternion.z);
                    float zw(quaternion.z * quaternion.w);
                    return -Math::Float3((2.0f * (xy - zw) * determinant), ((-xx + yy - zz + ww) * determinant), (2.0f * (yz + xw) * determinant));
                }
            }

            inline void updateClipRegionRoot(float tangentCoordinate, float lightCoordinate, float lightDepth, float radius, float radiusSquared, float lightRangeSquared, float cameraScale, float& minimum, float& maximum)
            {
                float nz = ((radius - tangentCoordinate * lightCoordinate) / lightDepth);
                float pz = ((lightRangeSquared - radiusSquared) / (lightDepth - (nz / tangentCoordinate) * lightCoordinate));
                if (pz > 0.0f)
                {
                    float clip = (-nz * cameraScale / tangentCoordinate);
                    if (tangentCoordinate > 0.0f)
                    {
                        // Left side boundary
                        minimum = std::max(minimum, clip);
                    }
                    else
                    {
                        // Right side boundary
                        maximum = std::min(maximum, clip);
                    }
                }
            }

            inline void updateClipRegion(float lightCoordinate, float lightDepth, float radius, float cameraScale, float& minimum, float& maximum)
            {
                float radiusSquared = (radius * radius);
                float lightDepthSquared = (lightDepth * lightDepth);
                float lightCoordinateSquared = (lightCoordinate * lightCoordinate);
                float lightRangeSquared = (lightCoordinateSquared + lightDepthSquared);
                float distanceSquared = ((radiusSquared * lightCoordinateSquared) - (lightRangeSquared * (radiusSquared - lightDepthSquared)));
                if (distanceSquared > 0.0f)
                {
                    float projectedRadius = (radius * lightCoordinate);
                    float distance = std::sqrt(distanceSquared);
                    float positiveTangent = ((projectedRadius + distance) / lightRangeSquared);
                    float negativeTangent = ((projectedRadius - distance) / lightRangeSquared);
                    updateClipRegionRoot(positiveTangent, lightCoordinate, lightDepth, radius, radiusSquared, lightRangeSquared, cameraScale, minimum, maximum);
                    updateClipRegionRoot(negativeTangent, lightCoordinate, lightDepth, radius, radiusSquared, lightRangeSquared, cameraScale, minimum, maximum);
                }
            }

            // Returns bounding box [min.xy, max.xy] in clip [-1, 1] space.
            inline Math::SIMD::Float4 getClipBounds(const Math::Float3 &position, float radius)
            {
                // Early out with empty rectangle if the light is too far behind the view frustum
                Math::SIMD::Float4 clipRegion(1.0f, 1.0f, 0.0f, 0.0f);
                if ((position.z + radius) >= nearClip)
                {
                    clipRegion.set(-1.0f, -1.0f, 1.0f, 1.0f);
                    updateClipRegion(position.x, position.z, radius, projectionMatrix.rx.x, clipRegion.minimum.x, clipRegion.maximum.x);
                    updateClipRegion(position.y, position.z, radius, projectionMatrix.ry.y, clipRegion.minimum.y, clipRegion.maximum.y);
                }

                return clipRegion;
            }

            bool isSeparated(float x, float y, float z, const Math::Float3 &position, float range)
            {
                // sub-frustrum bounds in view space       
                float minimumZ = (z - 0) * 1.0f / GridDepth * (farClip - nearClip) + nearClip;
                float maximumZ = (z + 1) * 1.0f / GridDepth * (farClip - nearClip) + nearClip;

                static const Math::SIMD::Float4 Negate(Math::Float2(-1.0f), Math::Float2(1.0f));
                static const Math::SIMD::Float4 GridDimensions(GridWidth, GridWidth, GridHeight, GridHeight);

                Math::SIMD::Float4 tileBounds(x, (x + 1.0f), y, (y + 1.0f));
                Math::SIMD::Float4 projectionScale(Math::Float2(projectionMatrix.rx.x), Math::Float2(projectionMatrix.ry.y));
                auto minimum = Negate * (Math::SIMD::Float4::One - Math::SIMD::Float4::Two / GridDimensions * tileBounds) * minimumZ / projectionScale;
                auto maximum = Negate * (Math::SIMD::Float4::One - Math::SIMD::Float4::Two / GridDimensions * tileBounds) * maximumZ / projectionScale;

                // heuristic plane separation test - works pretty well in practice
                Math::Float3 minimumZcenter((minimum.x + minimum.y) * 0.5f, (minimum.z + minimum.w) * 0.5f, minimumZ);
                Math::Float3 maximumZcenter((maximum.x + maximum.y) * 0.5f, (maximum.z + maximum.w) * 0.5f, maximumZ);
                Math::Float3 center((minimumZcenter + maximumZcenter) * 0.5f);
                Math::Float3 normal((center - position).getNormal());

                // compute distance of all corners to the tangent plane, with a few shortcuts (saves 14 muls)
                Math::Float2 tileCorners(-normal.dot(position));
                tileCorners.minimum += std::min(normal.x * minimum.x, normal.x * minimum.y);
                tileCorners.minimum += std::min(normal.y * minimum.z, normal.y * minimum.w);
                tileCorners.minimum += normal.z * minimumZ;
                tileCorners.maximum += std::min(normal.x * maximum.x, normal.x * maximum.y);
                tileCorners.maximum += std::min(normal.y * maximum.z, normal.y * maximum.w);
                tileCorners.maximum += normal.z * maximumZ;
                return (std::min(tileCorners.minimum, tileCorners.maximum) > range);
            }

            void addLightCluster(const Math::Float3 &position, float range, uint32_t lightIndex, bool pointLight)
            {
                auto flipBounds((getClipBounds(position, range) + Math::SIMD::Float4::One) * Math::SIMD::Float4::Half);
                Math::SIMD::Float4 clipBounds(flipBounds.x, (1.0f - flipBounds.w), flipBounds.z, (1.0f - flipBounds.y));

                static const Math::Int2 GridDimensions(GridWidth, GridHeight);
                Math::Int4 gridBounds(
                    int32_t(std::floor(clipBounds.x * GridWidth)),
                    int32_t(std::floor(clipBounds.y * GridHeight)),
                    int32_t(std::ceil(clipBounds.z * GridWidth)),
                    int32_t(std::ceil(clipBounds.w * GridHeight))
                );
                
                gridBounds[0] = (gridBounds[0] < 0 ? 0 : gridBounds[0]);
                gridBounds[1] = (gridBounds[1] < 0 ? 0 : gridBounds[1]);
                gridBounds[2] = (gridBounds[2] > GridWidth ? GridWidth : gridBounds[2]);
                gridBounds[3] = (gridBounds[3] > GridHeight ? GridHeight : gridBounds[3]);

                float centerDepth = ((position.z - nearClip) / (farClip - nearClip));
                float rangeDepth = (range / (farClip - nearClip));

                Math::Int2 depthBounds(
                    std::floor((centerDepth - rangeDepth) * GridDepth),
                    std::ceil((centerDepth + rangeDepth) * GridDepth)
                );

                depthBounds[0] = (depthBounds[0] < 0 ? 0 : depthBounds[0]);
                depthBounds[1] = (depthBounds[1] > GridDepth ? GridDepth : depthBounds[1]);

                concurrency::parallel_for(depthBounds.minimum, depthBounds.maximum, [&](auto z) -> void
                {
                    uint32_t zSlice = (z * GridHeight);
                    for (auto y = gridBounds.minimum.y; y < gridBounds.maximum.y; y++)
                    {
                        uint32_t ySlize = ((zSlice + y) * GridWidth);
                        for (auto x = gridBounds.minimum.x; x < gridBounds.maximum.x; x++)
                        {
                            if (!isSeparated(x, y, z, position, range))
                            {
                                uint32_t gridIndex = (ySlize + x);
                                auto &gridData = clusterLightsList[gridIndex];
                                if (pointLight)
                                {
                                    gridData.pointLightList.push_back(lightIndex);
                                }
                                else
                                {
                                    gridData.spotLightList.push_back(lightIndex);
                                }

                                InterlockedIncrement(&lightIndexCount);
                            }
                        }
                    }
                });
            }

            void addLight(Plugin::Entity *entity, const Components::PointLight &lightComponent)
            {
                auto &transformComponent = entity->getComponent<Components::Transform>();
                //if (viewFrustum.isVisible(Shapes::Sphere(transformComponent.position, lightComponent.range + lightComponent.radius)))
                {
                    auto &colorComponent = entity->getComponent<Components::Color>();

                    auto lightIterator = pointLightList.grow_by(1);
                    PointLightData &lightData = (*lightIterator);
                    lightData.radiance = colorComponent.value.xyz;
                    lightData.position = viewMatrix.transform(transformComponent.position);
                    lightData.radius = lightComponent.radius;
                    lightData.range = lightComponent.range;

                    auto lightIndex = std::distance(pointLightList.begin(), lightIterator);
                    addLightCluster(lightData.position, (lightData.radius + lightData.range), lightIndex, true);
                }
            }

            void addLight(Plugin::Entity *entity, const Components::SpotLight &lightComponent)
            {
                auto &transformComponent = entity->getComponent<Components::Transform>();
                //if (viewFrustum.isVisible(Shapes::Sphere(transformComponent.position, lightComponent.range)))
                {
                    auto &colorComponent = entity->getComponent<Components::Color>();

                    auto lightIterator = spotLightList.grow_by(1);
                    SpotLightData &lightData = (*lightIterator);
                    lightData.radiance = colorComponent.value.xyz;
                    lightData.position = viewMatrix.transform(transformComponent.position);
                    lightData.radius = lightComponent.radius;
                    lightData.range = lightComponent.range;
                    lightData.direction = viewMatrix.rotate(getLightDirection(transformComponent.rotation));
                    lightData.innerAngle = lightComponent.innerAngle;
                    lightData.outerAngle = lightComponent.outerAngle;
                    lightData.coneFalloff = lightComponent.coneFalloff;

                    auto lightIndex = std::distance(spotLightList.begin(), lightIterator);
                    addLightCluster(lightData.position, (lightData.radius + lightData.range), lightIndex, false);
                }
            }

            // Plugin::Population Slots
            void onLoadBegin(const String &populationName)
            {
                GEK_REQUIRE(resources);
                resources->clear();
                directionalLightEntities.clear();
                pointLightEntities.clear();
                spotLightEntities.clear();
            }

            void onLoadSucceeded(const String &populationName)
            {
                population->listEntities([&](Plugin::Entity *entity, const wchar_t *) -> void
                {
                    addEntity(entity);
                });
            }

            void onEntityCreated(Plugin::Entity *entity, const wchar_t *entityName)
            {
                addEntity(entity);
            }

            void onEntityDestroyed(Plugin::Entity *entity)
            {
                removeEntity(entity);
            }

            void onComponentAdded(Plugin::Entity *entity, const std::type_index &type)
            {
                addEntity(entity);
            }

            void onComponentRemoved(Plugin::Entity *entity, const std::type_index &type)
            {
                removeEntity(entity);
            }

            // Renderer
            Video::Device * getVideoDevice(void) const
            {
                return videoDevice;
            }

            void queueDrawCall(VisualHandle plugin, MaterialHandle material, std::function<void(Video::Device::Context *videoContext)> draw)
            {
                if (plugin && material && draw)
                {
                    ShaderHandle shader = resources->getMaterialShader(material);
                    if (shader)
                    {
                        drawCallList.push_back(DrawCallValue(material, plugin, shader, draw));
                    }
                }
            }

            void render(const Math::SIMD::Float4x4 &viewMatrix, const Math::SIMD::Float4x4 &projectionMatrix, float nearClip, float farClip, const std::vector<String> *filterList, ResourceHandle cameraTarget)
            {
                GEK_REQUIRE(videoDevice);
                GEK_REQUIRE(population);

                auto backBuffer = videoDevice->getBackBuffer();
                auto width = backBuffer->getWidth();
                auto height = backBuffer->getHeight();

                *const_cast<Shapes::Frustum *>(&this->viewFrustum) = Shapes::Frustum(viewMatrix * projectionMatrix);
                *const_cast<Math::SIMD::Float4x4 *>(&this->viewMatrix) = viewMatrix;
                *const_cast<Math::SIMD::Float4x4 *>(&this->projectionMatrix) = projectionMatrix;
                *const_cast<float *>(&this->nearClip) = nearClip;
                *const_cast<float *>(&this->farClip) = farClip;

                EngineConstantData engineConstantData;
                engineConstantData.frameTime = population->getFrameTime();
                engineConstantData.worldTime = population->getWorldTime();

                CameraConstantData cameraConstantData;
                cameraConstantData.fieldOfView.x = (1.0f / projectionMatrix._11);
                cameraConstantData.fieldOfView.y = (1.0f / projectionMatrix._22);
                cameraConstantData.nearClip = nearClip;
                cameraConstantData.farClip = farClip;
                cameraConstantData.viewMatrix = viewMatrix;
                cameraConstantData.projectionMatrix = projectionMatrix;

                drawCallList.clear();
                onRenderScene.emit(viewFrustum, viewMatrix);
                if (!drawCallList.empty())
                {
                    Video::Device::Context *videoContext = videoDevice->getDefaultContext();
                    videoContext->clearState();

                    videoDevice->updateResource(engineConstantBuffer.get(), &engineConstantData);
                    videoDevice->updateResource(cameraConstantBuffer.get(), &cameraConstantData);

                    std::vector<Video::Buffer *> bufferList = { engineConstantBuffer.get(), cameraConstantBuffer.get() };
                    videoContext->geometryPipeline()->setConstantBufferList(bufferList, 0);
                    videoContext->vertexPipeline()->setConstantBufferList(bufferList, 0);
                    videoContext->pixelPipeline()->setConstantBufferList(bufferList, 0);
                    videoContext->computePipeline()->setConstantBufferList(bufferList, 0);

                    std::vector<Video::Object *> samplerList = { pointSamplerState.get(), linearClampSamplerState.get(), linearWrapSamplerState.get() };
                    videoContext->pixelPipeline()->setSamplerStateList(samplerList, 0);

                    videoContext->setPrimitiveType(Video::PrimitiveType::TriangleList);

                    concurrency::parallel_sort(std::begin(drawCallList), std::end(drawCallList), [](const DrawCallValue &leftValue, const DrawCallValue &rightValue) -> bool
                    {
                        return (leftValue.value < rightValue.value);
                    });

                    bool isLightingRequired = false;

                    ShaderHandle currentShader;
                    std::map<uint32_t, std::vector<DrawCallSet>> drawCallSetMap;
                    for (auto &drawCall = std::begin(drawCallList); drawCall != std::end(drawCallList); )
                    {
                        currentShader = drawCall->shader;

                        auto beginShaderList = drawCall;
                        while (drawCall != std::end(drawCallList) && drawCall->shader == currentShader)
                        {
                            ++drawCall;
                        };

                        auto endShaderList = drawCall;
                        Engine::Shader *shader = resources->getShader(currentShader);
                        if (!shader)
                        {
                            continue;
                        }

                        auto &shaderList = drawCallSetMap[shader->getPriority()];
                        shaderList.push_back(DrawCallSet(shader, beginShaderList, endShaderList));
                        for (auto pass = shader->begin(videoContext, cameraConstantData.viewMatrix, viewFrustum); pass; pass = pass->next())
                        {
                            isLightingRequired |= pass->isLightingRequired();
                        }
                    }

                    if (isLightingRequired)
                    {
                        lightIndexCount = 0;
                        concurrency::parallel_for_each(std::begin(clusterLightsList), std::end(clusterLightsList), [&](auto &gridData) -> void
                        {
                            gridData.pointLightList.clear();
                            gridData.spotLightList.clear();
                        });

                        auto directionalLightsDone = threadPool.enqueue([&](void) -> void
                        {
                            directionalLightList.clear();
                            directionalLightList.reserve(directionalLightEntities.size());
                            for (auto &entity : directionalLightEntities)
                            {
                                auto &transformComponent = entity->getComponent<Components::Transform>();
                                auto &colorComponent = entity->getComponent<Components::Color>();
                                auto &lightComponent = entity->getComponent<Components::DirectionalLight>();

                                DirectionalLightData lightData;
                                lightData.radiance = colorComponent.value.xyz;
                                lightData.direction = viewMatrix.rotate(getLightDirection(transformComponent.rotation));
                                directionalLightList.push_back(lightData);
                            }

                            if (!directionalLightList.empty())
                            {
                                if (!directionalLightDataBuffer || directionalLightDataBuffer->getCount() < directionalLightList.size())
                                {
                                    directionalLightDataBuffer = videoDevice->createBuffer(sizeof(DirectionalLightData), directionalLightList.size(), Video::BufferType::Structured, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                                    directionalLightDataBuffer->setName(L"directionalLightDataBuffer");
                                }
                            }
                        });

                        auto pointLightsDone = threadPool.enqueue([&](void) -> void
                        {
                            pointLightList.clear();
                            concurrency::parallel_for_each(std::begin(pointLightEntities), std::end(pointLightEntities), [&](Plugin::Entity *entity) -> void
                            {
                                auto &lightComponent = entity->getComponent<Components::PointLight>();
                                addLight(entity, lightComponent);
                            });

                            if (!pointLightList.empty())
                            {
                                if (!pointLightDataBuffer || pointLightDataBuffer->getCount() < pointLightList.size())
                                {
                                    pointLightDataBuffer = videoDevice->createBuffer(sizeof(PointLightData), pointLightList.size(), Video::BufferType::Structured, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                                    pointLightDataBuffer->setName(L"pointLightDataBuffer");
                                }
                            }
                        });

                        auto spotLightsDone = threadPool.enqueue([&](void) -> void
                        {
                            spotLightList.clear();
                            concurrency::parallel_for_each(std::begin(spotLightEntities), std::end(spotLightEntities), [&](Plugin::Entity *entity) -> void
                            {
                                auto &lightComponent = entity->getComponent<Components::SpotLight>();
                                addLight(entity, lightComponent);
                            });

                            if (!spotLightList.empty())
                            {
                                if (!spotLightDataBuffer || spotLightDataBuffer->getCount() < spotLightList.size())
                                {
                                    spotLightDataBuffer = videoDevice->createBuffer(sizeof(SpotLightData), spotLightList.size(), Video::BufferType::Structured, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                                    spotLightDataBuffer->setName(L"spotLightDataBuffer");
                                }
                            }
                        });

                        directionalLightsDone.get();
                        pointLightsDone.get();
                        spotLightsDone.get();

                        clusterIndexList.clear();
                        clusterIndexList.reserve(lightIndexCount);
                        for (uint32_t tileIndex = 0; tileIndex < GridSize; tileIndex++)
                        {
                            auto &clusterData = clusterDataList[tileIndex];
                            auto &clusterLights = clusterLightsList[tileIndex];
                            clusterData.indexOffset = clusterIndexList.size();
                            clusterData.pointLightCount = clusterLights.pointLightList.size();
                            clusterData.spotLightCount = clusterLights.spotLightList.size();
                            clusterIndexList.insert(std::end(clusterIndexList), std::begin(clusterLights.pointLightList), std::end(clusterLights.pointLightList));
                            clusterIndexList.insert(std::end(clusterIndexList), std::begin(clusterLights.spotLightList), std::end(clusterLights.spotLightList));
                        }

                        if (!directionalLightList.empty())
                        {
                            DirectionalLightData *directionalLightData = nullptr;
                            videoDevice->mapBuffer(directionalLightDataBuffer.get(), directionalLightData);
                            std::copy(std::begin(directionalLightList), std::end(directionalLightList), directionalLightData);
                            videoDevice->unmapBuffer(directionalLightDataBuffer.get());
                        }

                        if (!pointLightList.empty())
                        {
                            PointLightData *pointLightData = nullptr;
                            videoDevice->mapBuffer(pointLightDataBuffer.get(), pointLightData);
                            std::copy(std::begin(pointLightList), std::end(pointLightList), pointLightData);
                            videoDevice->unmapBuffer(pointLightDataBuffer.get());
                        }

                        if (!spotLightList.empty())
                        {
                            SpotLightData *spotLightData = nullptr;
                            videoDevice->mapBuffer(spotLightDataBuffer.get(), spotLightData);
                            std::copy(std::begin(spotLightList), std::end(spotLightList), spotLightData);
                            videoDevice->unmapBuffer(spotLightDataBuffer.get());
                        }

                        ClusterData *clusterDataData = nullptr;
                        videoDevice->mapBuffer(clusterDataBuffer.get(), clusterDataData);
                        std::copy(std::begin(clusterDataList), std::end(clusterDataList), clusterDataData);
                        videoDevice->unmapBuffer(clusterDataBuffer.get());

                        if (!clusterIndexList.empty())
                        {
                            if (!clusterIndexBuffer || clusterIndexBuffer->getCount() < clusterIndexList.size())
                            {
                                clusterIndexBuffer = videoDevice->createBuffer(Video::Format::R32_UINT, clusterIndexList.size(), Video::BufferType::Raw, Video::BufferFlags::Mappable | Video::BufferFlags::Resource);
                                clusterIndexBuffer->setName(L"clusterIndexBuffer");
                            }

                            uint32_t *clusterIndexData = nullptr;
                            videoDevice->mapBuffer(clusterIndexBuffer.get(), clusterIndexData);
                            std::copy(std::begin(clusterIndexList), std::end(clusterIndexList), clusterIndexData);
                            videoDevice->unmapBuffer(clusterIndexBuffer.get());
                        }

                        LightConstantData lightConstants;
                        lightConstants.directionalLightCount = directionalLightList.size();
                        lightConstants.pointLightCount = pointLightList.size();
                        lightConstants.spotLightCount = spotLightList.size();
                        lightConstants.gridSize.x = GridWidth;
                        lightConstants.gridSize.y = GridHeight;
                        lightConstants.gridSize.z = GridDepth;
                        lightConstants.tileSize.x = (width / GridWidth);
                        lightConstants.tileSize.y = (height / GridHeight);
                        videoDevice->updateResource(lightConstantBuffer.get(), &lightConstants);
                    }

                    for (auto &shaderDrawCallList : drawCallSetMap)
                    {
                        for (auto &shaderDrawCall : shaderDrawCallList.second)
                        {
                            auto &shader = shaderDrawCall.shader;
                            for (auto pass = shader->begin(videoContext, cameraConstantData.viewMatrix, viewFrustum); pass; pass = pass->next())
                            {
                                resources->startResourceBlock();
                                if (pass->isLightingRequired())
                                {
                                    videoContext->pixelPipeline()->setConstantBufferList({ lightConstantBuffer.get() }, 3);
                                    videoContext->pixelPipeline()->setResourceList(
                                    {
                                        directionalLightDataBuffer.get(),
                                        pointLightDataBuffer.get(),
                                        spotLightDataBuffer.get(),
                                        clusterDataBuffer.get(),
                                        clusterIndexBuffer.get()
                                    }, 0);
                                }

                                switch (pass->prepare())
                                {
                                case Engine::Shader::Pass::Mode::Forward:
                                    if (true)
                                    {
                                        VisualHandle currentVisual;
                                        MaterialHandle currentMaterial;
                                        for (auto drawCall = shaderDrawCall.begin; drawCall != shaderDrawCall.end; ++drawCall)
                                        {
                                            if (currentVisual != drawCall->plugin)
                                            {
                                                currentVisual = drawCall->plugin;
                                                resources->setVisual(videoContext, currentVisual);
                                            }

                                            if (currentMaterial != drawCall->material)
                                            {
                                                currentMaterial = drawCall->material;
                                                resources->setMaterial(videoContext, pass.get(), currentMaterial);
                                            }

                                            drawCall->onDraw(videoContext);
                                        }
                                    }

                                    break;

                                case Engine::Shader::Pass::Mode::Deferred:
                                    videoContext->vertexPipeline()->setProgram(deferredVertexProgram.get());
                                    resources->drawPrimitive(videoContext, 3, 0);
                                    break;

                                case Engine::Shader::Pass::Mode::Compute:
                                    break;
                                };

                                pass->clear();
                                if (pass->isLightingRequired())
                                {
                                    videoContext->pixelPipeline()->clearResourceList(5, 0);
                                    videoContext->pixelPipeline()->clearConstantBufferList(1, 3);
                                }
                            }
                        }
                    }

                    if (filterList)
                    {
                        videoContext->vertexPipeline()->setProgram(deferredVertexProgram.get());
                        for (auto &filterName : *filterList)
                        {
                            Engine::Filter * const filter = resources->getFilter(filterName);
                            if (filter)
                            {
                                for (auto pass = filter->begin(videoContext); pass; pass = pass->next())
                                {
                                    switch (pass->prepare())
                                    {
                                    case Engine::Filter::Pass::Mode::Deferred:
                                        resources->drawPrimitive(videoContext, 3, 0);
                                        break;

                                    case Engine::Filter::Pass::Mode::Compute:
                                        break;
                                    };

                                    pass->clear();
                                }
                            }
                        }
                    }

                    videoContext->geometryPipeline()->clearConstantBufferList(2, 0);
                    videoContext->vertexPipeline()->clearConstantBufferList(2, 0);
                    videoContext->pixelPipeline()->clearConstantBufferList(2, 0);
                    videoContext->computePipeline()->clearConstantBufferList(2, 0);

                    if (cameraTarget)
                    {
                        renderOverlay(videoContext, resources->getResourceHandle(L"screen"), cameraTarget);
                    }
                }
            }

            void renderOverlay(Video::Device::Context *videoContext, ResourceHandle input, ResourceHandle target)
            {
                videoContext->setBlendState(blendState.get(), Math::Float4::Black, 0xFFFFFFFF);
                videoContext->setDepthState(depthState.get(), 0);
                videoContext->setRenderState(renderState.get());

                videoContext->setPrimitiveType(Video::PrimitiveType::TriangleList);

                resources->startResourceBlock();
                resources->setResourceList(videoContext->pixelPipeline(), { input }, 0);

                videoContext->vertexPipeline()->setProgram(deferredVertexProgram.get());
                videoContext->pixelPipeline()->setProgram(deferredPixelProgram.get());
                if (target)
                {
                    resources->setRenderTargetList(videoContext, { target }, nullptr);
                }
                else
                {
                    resources->setBackBuffer(videoContext, nullptr);
                }

                resources->drawPrimitive(videoContext, 3, 0);
            }
        };

        GEK_REGISTER_CONTEXT_USER(Renderer);
    }; // namespace Implementation
}; // namespace Gek
