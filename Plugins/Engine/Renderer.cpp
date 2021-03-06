﻿#include "GEK/Math/SIMD.hpp"
#include "GEK/Utility/String.hpp"
#include "GEK/Utility/FileSystem.hpp"
#include "GEK/Utility/JSON.hpp"
#include "GEK/Utility/ContextUser.hpp"
#include "GEK/Utility/ThreadPool.hpp"
#include "GEK/Utility/Allocator.hpp"
#include "GEK/Engine/Core.hpp"
#include "GEK/Engine/Renderer.hpp"
#include "GEK/Engine/Resources.hpp"
#include "GEK/Engine/Visual.hpp"
#include "GEK/Engine/Shader.hpp"
#include "GEK/Engine/Filter.hpp"
#include "GEK/Engine/Material.hpp"
#include "GEK/Engine/Population.hpp"
#include "GEK/Engine/Entity.hpp"
#include "GEK/Engine/Component.hpp"
#include "GEK/Engine/ComponentMixin.hpp"
#include "GEK/Components/Transform.hpp"
#include "GEK/Components/Color.hpp"
#include "GEK/Components/Light.hpp"
#include "GEK/Shapes/Sphere.hpp"
#include <concurrent_unordered_set.h>
#include <concurrent_vector.h>
#include <concurrent_queue.h>
#include <smmintrin.h>
#include <algorithm>
#include <ppl.h>

namespace Gek
{
    namespace Implementation
    {
        static const int32_t GridWidth = 16;
        static const int32_t GridHeight = 8;
        static const int32_t GridDepth = 24;
        static const int32_t GridSize = (GridWidth * GridHeight * GridDepth);
        static const Math::Float4 Negate(Math::Float2(-1.0f), Math::Float2(1.0f));
        static const Math::Float4 GridDimensions(GridWidth, GridWidth, GridHeight, GridHeight);

        GEK_CONTEXT_USER(Renderer, Plugin::Core *)
            , public Plugin::Renderer
        {
        public:
            struct DirectionalLightData
            {
                Math::Float3 radiance;
                float padding1;
                Math::Float3 direction;
                float padding2;
            };

            struct PointLightData
            {
                Math::Float3 radiance;
                float radius;
                Math::Float3 position;
                float range;
            };

            struct SpotLightData
            {
                Math::Float3 radiance;
                float radius;
                Math::Float3 position;
                float range;
                Math::Float3 direction;
                float padding1;
                float innerAngle;
                float outerAngle;
                float coneFalloff;
                float padding2;
            };

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
                Math::Float4x4 viewMatrix;
                Math::Float4x4 projectionMatrix;
            };

            struct LightConstantData
            {
                Math::UInt3 gridSize;
                uint32_t directionalLightCount;
                Math::UInt2 tileSize;
                uint32_t pointLightCount;
                uint32_t spotLightCount;
            };

            struct TileLightIndex
            {
                concurrency::concurrent_vector<uint16_t> pointLightList;
                concurrency::concurrent_vector<uint16_t> spotLightList;
            };

            struct TileOffsetCount
            {
                uint32_t indexOffset;
                uint16_t pointLightCount;
                uint16_t spotLightCount;
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

                DrawCallValue(MaterialHandle material, VisualHandle plugin, ShaderHandle shader, std::function<void(Video::Device::Context *)> &&onDraw)
                    : material(material)
                    , plugin(plugin)
                    , shader(shader)
                    , onDraw(std::move(onDraw))
                {
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
            };

            struct Camera
            {
                std::string name;
                Shapes::Frustum viewFrustum;
                Math::Float4x4 viewMatrix;
                Math::Float4x4 projectionMatrix;
                float nearClip = 0.0f;
                float farClip = 0.0f;
                ResourceHandle cameraTarget;
                ShaderHandle forceShader;

                Camera(void)
                {
                }

                Camera(const Camera &renderCall)
                    : viewFrustum(renderCall.viewFrustum)
                    , viewMatrix(renderCall.viewMatrix)
                    , projectionMatrix(renderCall.projectionMatrix)
                    , nearClip(renderCall.nearClip)
                    , farClip(renderCall.farClip)
                    , cameraTarget(renderCall.cameraTarget)
                    , forceShader(renderCall.forceShader)
                {
                }
            };

            template <typename COMPONENT, typename DATA>
            struct LightData
            {
                Video::Device *videoDevice = nullptr;
                std::vector<Plugin::Entity *> entityList;
                concurrency::concurrent_vector<DATA, AlignedAllocator<DATA, 16>> lightList;
                Video::BufferPtr lightDataBuffer;

                concurrency::critical_section addSection;
                concurrency::critical_section removeSection;

                LightData(size_t reserve, Video::Device *videoDevice)
                    : videoDevice(videoDevice)
                {
                    lightList.reserve(reserve);
                    createBuffer();
                }

                void addEntity(Plugin::Entity * const entity)
                {
                    if (entity->hasComponent<COMPONENT>())
                    {
                        concurrency::critical_section::scoped_lock lock(addSection);
                        auto search = std::find_if(std::begin(entityList), std::end(entityList), [entity](Plugin::Entity * const search) -> bool
                        {
                            return (entity == search);
                        });

                        if (search == std::end(entityList))
                        {
                            entityList.push_back(entity);
                        }
                    }
                }

                void removeEntity(Plugin::Entity * const entity)
                {
                    concurrency::critical_section::scoped_lock lock(removeSection);
                    auto search = std::find_if(std::begin(entityList), std::end(entityList), [entity](Plugin::Entity * const search) -> bool
                    {
                        return (entity == search);
                    });

                    if (search != std::end(entityList))
                    {
                        entityList.erase(search);
                    }
                }

                void clearEntities(void)
                {
                    entityList.clear();
                }

                void createBuffer(void)
                {
                    if (!lightDataBuffer || lightDataBuffer->getDescription().count < lightList.size())
                    {
                        lightDataBuffer = nullptr;

                        Video::Buffer::Description lightBufferDescription;
                        lightBufferDescription.type = Video::Buffer::Type::Structured;
                        lightBufferDescription.flags = Video::Buffer::Flags::Mappable | Video::Buffer::Flags::Resource;

                        lightBufferDescription.stride = sizeof(DATA);
                        lightBufferDescription.count = lightList.capacity();
                        lightDataBuffer = videoDevice->createBuffer(lightBufferDescription);
                        lightDataBuffer->setName(String::Format("render:%v", typeid(COMPONENT).name()));
                    }
                }

                bool updateBuffer(void)
                {
                    bool updated = true;
                    if (!lightList.empty())
                    {
                        DATA *lightData = nullptr;
                        if (updated = videoDevice->mapBuffer(lightDataBuffer.get(), lightData))
                        {
                            std::copy(std::begin(lightList), std::end(lightList), lightData);
                            videoDevice->unmapBuffer(lightDataBuffer.get());
                        }
                    }

                    return updated;
                }
            };

            template <typename COMPONENT, typename DATA>
            struct LightVisibilityData
                : public LightData<COMPONENT, DATA>
            {
                std::vector<float, AlignedAllocator<float, 16>> shapeXPositionList;
                std::vector<float, AlignedAllocator<float, 16>> shapeYPositionList;
                std::vector<float, AlignedAllocator<float, 16>> shapeZPositionList;
                std::vector<float, AlignedAllocator<float, 16>> shapeRadiusList;
                std::vector<bool> visibilityList;

                LightVisibilityData(size_t reserve, Video::Device *videoDevice)
                    : LightData(reserve, videoDevice)
                {
                }

                void clearEntities(void)
                {
                    LightData::clearEntities();
                    shapeXPositionList.clear();
                    shapeYPositionList.clear();
                    shapeZPositionList.clear();
                    shapeRadiusList.clear();
                    visibilityList.clear();
                }

                void update(Video::Device *videoDevice, Math::SIMD::Frustum const &frustum, const std::function<void(Plugin::Entity * const, const COMPONENT &)> &addLight)
                {
                    const auto entityCount = entityList.size();
                    auto buffer = (entityCount % 4);
                    buffer = (buffer ? (4 - buffer) : buffer);
                    auto bufferedEntityCount = (entityCount + buffer);
                    shapeXPositionList.resize(bufferedEntityCount);
                    shapeYPositionList.resize(bufferedEntityCount);
                    shapeZPositionList.resize(bufferedEntityCount);
                    shapeRadiusList.resize(bufferedEntityCount);
                    for (size_t entityIndex = 0; entityIndex < entityCount; ++entityIndex)
                    {
                        auto entity = entityList[entityIndex];
                        auto &transformComponent = entity->getComponent<Components::Transform>();
                        auto &lightComponent = entity->getComponent<COMPONENT>();

                        shapeXPositionList[entityIndex] = transformComponent.position.x;
                        shapeYPositionList[entityIndex] = transformComponent.position.y;
                        shapeZPositionList[entityIndex] = transformComponent.position.z;
                        shapeRadiusList[entityIndex] = (lightComponent.range + lightComponent.radius);
                    }

                    visibilityList.resize(bufferedEntityCount);
                    Math::SIMD::cullSpheres(frustum, bufferedEntityCount, shapeXPositionList, shapeYPositionList, shapeZPositionList, shapeRadiusList, visibilityList);

                    lightList.clear();
                    concurrency::parallel_for(size_t(0), entityList.size(), [&](size_t index) -> void
                    {
                        if (visibilityList[index])
                        {
                            auto entity = entityList[index];
                            auto &lightComponent = entity->getComponent<COMPONENT>();
                            addLight(entity, lightComponent);
                        }
                    });

                    if (!lightList.empty())
                    {
                        createBuffer();
                    }
                }
            };

            class Profiler
            {
            public:
                enum Flags
                {
                    Hide = 1 << 0,
                    PreIndent = 1 << 1,
                    PostIndent = 1 << 2,
                    PreUnindent = 1 << 3,
                    PostUnindent = 1 << 4,
                };

            public:
                virtual ~Profiler(void) = default;

                virtual int updateEventData(void) { return -1; };
                virtual void showEventData(int currentFrame) { };

                virtual void beginFrame(void) { };
                virtual void timeStamp(std::string const &name, uint32_t flags = 0) { };
                virtual void endFrame(void) { };
                virtual void waitAndUpdate(void) { };
            };

            class GPUProfiler
                : public Profiler
            {
            public:
                struct EventData
                {
                    std::array<Video::QueryPtr, 3> eventList;
                    std::list<std::pair<Video::Query::TimeStamp, float>> frameList;
                    float previousFrameTime = 0.0f;
                    float averageFrameTime = 0.0f;
                    float totalAverageTime = 0.0f;
                    uint32_t flags = 0;
                    ImColor color;

                    EventData(uint32_t flags = 0)
                        : flags(flags)
                        , color(rand() % 255, rand() % 255, rand() % 255)
                    {
                    }
                };

            private:
                Video::Device *videoDevice = nullptr;
                Video::Device::Context *videoContext = nullptr;
                std::array<Video::QueryPtr, 3> disjointList;
                using EventMap = std::unordered_map<std::string, EventData>;
                EventMap eventMap;

                int frameQuery = 0;
                int frameUpdate = -1;
                int frameCountAverage = 0;
                std::chrono::time_point<std::chrono::steady_clock> nextFrameTime;
                std::array<std::vector<typename EventMap::value_type *>, 3> eventTimeline;

            public:
                GPUProfiler(Video::Device *videoDevice)
                    : videoDevice(videoDevice)
                    , videoContext(videoDevice->getDefaultContext())
                {
                    disjointList[0] = videoDevice->createQuery(Video::Query::Type::DisjointTimeStamp);
                    disjointList[1] = videoDevice->createQuery(Video::Query::Type::DisjointTimeStamp);
                    disjointList[2] = videoDevice->createQuery(Video::Query::Type::DisjointTimeStamp);
                }

                int updateEventData(void)
                {
                    if (frameUpdate < 0)
                    {
                        frameUpdate = 0;
                        return -1;
                    }

                    int currentFrame = frameUpdate;
                    ++frameUpdate %= 3;

                    Video::Query::DisjointTimeStamp timestampDisjoint;
                    if (videoContext->getData(disjointList[currentFrame].get(), &timestampDisjoint, sizeof(Video::Query::DisjointTimeStamp), true) == Video::Query::Status::Ready)
                    {
                        if (!timestampDisjoint.disjoint)
                        {
                            Video::Query::TimeStamp previousTimeStamp = 0;
                            for (auto &eventSearch : eventTimeline[currentFrame])
                            {
                                Video::Query::TimeStamp currentTimeStamp = 0;
                                auto &eventData = eventSearch->second;
                                if (videoContext->getData(eventData.eventList[currentFrame].get(), &currentTimeStamp, sizeof(Video::Query::TimeStamp), true) == Video::Query::Status::Ready)
                                {
                                    Video::Query::TimeStamp deltaTime = currentTimeStamp;
                                    deltaTime -= (previousTimeStamp == 0 ? currentTimeStamp : previousTimeStamp);
                                    eventData.previousFrameTime = float(deltaTime) / float(timestampDisjoint.frequency);
                                    eventData.totalAverageTime += eventData.previousFrameTime;
                                    eventData.frameList.push_back(std::make_pair(currentTimeStamp, eventData.previousFrameTime));
                                    if (eventData.frameList.size() > 100)
                                    {
                                        eventData.frameList.pop_front();
                                    }

                                    previousTimeStamp = currentTimeStamp;
                                }
                                else
                                {
                                    LockedWrite{ std::cerr } << "Couldn't retrieve timestamp query data";
                                }
                            }
                        }
                        else
                        {
                            LockedWrite{ std::cerr } << "Disjoint timestamps encountered, skipping";
                        }
                    }
                    else
                    {
                        LockedWrite{ std::cerr } << "Couldn't retrieve disjoint query data";
                    }

                    ++frameCountAverage;
                    if (std::chrono::high_resolution_clock::now() > nextFrameTime)
                    {
                        for (auto &eventSearch : eventMap)
                        {
                            eventSearch.second.averageFrameTime = eventSearch.second.totalAverageTime / frameCountAverage;
                            eventSearch.second.totalAverageTime = 0.0f;
                        }

                        frameCountAverage = 0;
                        nextFrameTime = (std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(500));
                    }

                    return currentFrame;
                }

                void showEventData(int currentFrame)
                {
                    float maximumTime = 0.0f;
                    for (auto &eventSearch : eventTimeline[currentFrame])
                    {
                        for (auto &eventFrame : eventSearch->second.frameList)
                        {
                            maximumTime = std::max(maximumTime, eventFrame.second);
                        }
                    }

                    for (auto &eventSearch : eventTimeline[currentFrame])
                    {
                        ImGui::PlotHistogram("##", [](void *data, int index) -> float
                        {
                            auto &frameList = *(std::list<std::pair<Video::Query::TimeStamp, float>> *)data;
                            return std::next(std::begin(frameList), index)->second;
                        }, &eventSearch->second.frameList, 100, 0, eventSearch->first.c_str(), 0.0f, 0.1f, ImVec2(ImGui::GetContentRegionAvailWidth(), 30.0f));
                    } 

                    auto showFloat = [](std::string const &label, float value)
                    {
                        auto labelEndPosition = label.find("##");
                        auto labelEnd = (labelEndPosition == std::string::npos ? nullptr : label.c_str() + labelEndPosition);
                        ImGui::AlignFirstTextHeightToWidgets();
                        ImGui::TextUnformatted(label.c_str(), labelEnd);

                        ImGui::SameLine();
                        ImGui::PushItemWidth(-1.0f);
                        ImGui::InputFloat("##", &value, 0.0f, 0.0f, -1.0f, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemWidth();
                    };

                    std::list<float> indentTime;
                    indentTime.push_back(0.0f);
                    for (auto &eventSearch : eventTimeline[currentFrame])
                    {
                        for (auto &time : indentTime)
                        {
                            time += eventSearch->second.averageFrameTime;
                        }

                        if (eventSearch->second.flags & Flags::PreIndent)
                        {
                            indentTime.push_back(0.0f);
                            ImGui::Indent();
                        }
                        else if (eventSearch->second.flags & Flags::PreUnindent)
                        {
                            ImGui::Unindent();
                            showFloat(eventSearch->first, indentTime.back());
                            indentTime.pop_back();
                        }

                        if (!(eventSearch->second.flags & Flags::Hide))
                        {
                            showFloat(eventSearch->first, eventSearch->second.averageFrameTime);
                        }

                        if (eventSearch->second.flags & Flags::PostIndent)
                        {
                            indentTime.push_back(0.0f);
                            ImGui::Indent();
                        }
                        else if (eventSearch->second.flags & Flags::PostUnindent)
                        {
                            showFloat(eventSearch->first, indentTime.back());
                            indentTime.pop_back();
                            ImGui::Unindent();
                        }
                    }

                    showFloat("Frame Time", indentTime.front());
                }

                void beginFrame(void)
                {
                    eventTimeline[frameQuery].clear();
                    videoContext->begin(disjointList[frameQuery].get());
                    timeStamp("**beginFrame", Flags::Hide);
                }

                void timeStamp(std::string const &name, uint32_t flags)
                {
                    auto eventSearch = eventMap.insert(std::make_pair(name, EventData(flags)));
                    auto &eventPair = *eventSearch.first;
                    auto &eventData = eventPair.second;
                    if (eventSearch.second)
                    {
                        eventData.eventList[0] = videoDevice->createQuery(Video::Query::Type::TimeStamp);
                        eventData.eventList[1] = videoDevice->createQuery(Video::Query::Type::TimeStamp);
                        eventData.eventList[2] = videoDevice->createQuery(Video::Query::Type::TimeStamp);
                        eventData.frameList.resize(100);
                    }

                    eventTimeline[frameQuery].push_back(&eventPair);
                    videoContext->end(eventData.eventList[frameQuery].get());
                }

                void endFrame(void)
                {
                    timeStamp("**endFrame", Flags::Hide);
                    videoContext->end(disjointList[frameQuery].get());
                    ++frameQuery %= 3;
                }
            };

        private:
            Plugin::Core *core = nullptr;
            Video::Device *videoDevice = nullptr;
            Plugin::Population *population = nullptr;
            Engine::Resources *resources = nullptr;

            std::unique_ptr<Profiler> profiler;
            bool showDebugInformation = false;

            Video::SamplerStatePtr bufferSamplerState;
            Video::SamplerStatePtr textureSamplerState;
            Video::SamplerStatePtr mipMapSamplerState;
            std::vector<Video::Object *> samplerList;

            Video::BufferPtr engineConstantBuffer;
            Video::BufferPtr cameraConstantBuffer;
            std::vector<Video::Buffer *> shaderBufferList;
            std::vector<Video::Buffer *> filterBufferList;
            std::vector<Video::Buffer *> lightBufferList;
            std::vector<Video::Object *> lightResoruceList;

            Video::ObjectPtr deferredVertexProgram;
            Video::ObjectPtr deferredPixelProgram;
            Video::BlendStatePtr blendState;
            Video::RenderStatePtr renderState;
            Video::DepthStatePtr depthState;

            ThreadPool workerPool;
            LightData<Components::DirectionalLight, DirectionalLightData> directionalLightData;
            LightVisibilityData<Components::PointLight, PointLightData> pointLightData;
            LightVisibilityData<Components::SpotLight, SpotLightData> spotLightData;

            TileLightIndex tileLightIndexList[GridSize];
            TileOffsetCount tileOffsetCountList[GridSize];
            std::vector<uint16_t> lightIndexList;
            uint32_t lightIndexCount = 0;

            Video::BufferPtr lightConstantBuffer;
            Video::BufferPtr tileOffsetCountBuffer;
            Video::BufferPtr lightIndexBuffer;

            DrawCallList drawCallList;
            concurrency::concurrent_queue<Camera> cameraQueue;
            Camera currentCamera;

            std::string screenOutput;

            struct
            {
                Video::ObjectPtr vertexProgram;
                Video::ObjectPtr inputLayout;
                Video::BufferPtr constantBuffer;
                Video::ObjectPtr pixelProgram;
                Video::ObjectPtr blendState;
                Video::ObjectPtr renderState;
                Video::ObjectPtr depthState;
                Video::TexturePtr fontTexture;
                Video::BufferPtr vertexBuffer;
                Video::BufferPtr indexBuffer;
            } gui;

        public:
            Renderer(Context *context, Plugin::Core *core)
                : ContextRegistration(context)
                , core(core)
                , videoDevice(core->getVideoDevice())
                , population(core->getPopulation())
                , resources(dynamic_cast<Engine::Resources *>(core->getResources()))
                , workerPool(3)
                , directionalLightData(10, core->getVideoDevice())
                , pointLightData(200, core->getVideoDevice())
                , spotLightData(200, core->getVideoDevice())
                , profiler(std::make_unique<GPUProfiler>(videoDevice))
            {
                population->onReset.connect(this, &Renderer::onReset);
                population->onEntityCreated.connect(this, &Renderer::onEntityCreated);
                population->onEntityDestroyed.connect(this, &Renderer::onEntityDestroyed);
                population->onComponentAdded.connect(this, &Renderer::onComponentAdded);
                population->onComponentRemoved.connect(this, &Renderer::onComponentRemoved);
                population->onUpdate[1000].connect(this, &Renderer::onUpdate);

                initializeSystem();
                initializeUI();
            }

            void initializeSystem(void)
            {
                LockedWrite{ std::cout } << "Initializing rendering system components";

                Video::SamplerState::Description bufferSamplerStateData;
                bufferSamplerStateData.filterMode = Video::SamplerState::FilterMode::MinificationMagnificationMipMapPoint;
                bufferSamplerStateData.addressModeU = Video::SamplerState::AddressMode::Clamp;
                bufferSamplerStateData.addressModeV = Video::SamplerState::AddressMode::Clamp;
                bufferSamplerState = videoDevice->createSamplerState(bufferSamplerStateData);
                bufferSamplerState->setName("renderer:bufferSamplerState");

                Video::SamplerState::Description textureSamplerStateData;
                textureSamplerStateData.maximumAnisotropy = 8;
                textureSamplerStateData.filterMode = Video::SamplerState::FilterMode::Anisotropic;
                textureSamplerStateData.addressModeU = Video::SamplerState::AddressMode::Wrap;
                textureSamplerStateData.addressModeV = Video::SamplerState::AddressMode::Wrap;
                textureSamplerState = videoDevice->createSamplerState(textureSamplerStateData);
                textureSamplerState->setName("renderer:textureSamplerState");

                Video::SamplerState::Description mipMapSamplerStateData;
                mipMapSamplerStateData.maximumAnisotropy = 8;
                mipMapSamplerStateData.filterMode = Video::SamplerState::FilterMode::MinificationMagnificationMipMapLinear;
                mipMapSamplerStateData.addressModeU = Video::SamplerState::AddressMode::Clamp;
                mipMapSamplerStateData.addressModeV = Video::SamplerState::AddressMode::Clamp;
                mipMapSamplerState = videoDevice->createSamplerState(mipMapSamplerStateData);
                mipMapSamplerState->setName("renderer:mipMapSamplerState");

                samplerList = { textureSamplerState.get(), mipMapSamplerState.get(), };

                Video::BlendState::Description blendStateInformation;
                blendState = videoDevice->createBlendState(blendStateInformation);
                blendState->setName("renderer:blendState");

                Video::RenderState::Description renderStateInformation;
                renderState = videoDevice->createRenderState(renderStateInformation);
                renderState->setName("renderer:renderState");

                Video::DepthState::Description depthStateInformation;
                depthState = videoDevice->createDepthState(depthStateInformation);
                depthState->setName("renderer:depthState");

                Video::Buffer::Description constantBufferDescription;
                constantBufferDescription.stride = sizeof(EngineConstantData);
                constantBufferDescription.count = 1;
                constantBufferDescription.type = Video::Buffer::Type::Constant;
                engineConstantBuffer = videoDevice->createBuffer(constantBufferDescription);
                engineConstantBuffer->setName("renderer:engineConstantBuffer");

                constantBufferDescription.stride = sizeof(CameraConstantData);
                cameraConstantBuffer = videoDevice->createBuffer(constantBufferDescription);
                cameraConstantBuffer->setName("renderer:cameraConstantBuffer");

                shaderBufferList = { engineConstantBuffer.get(), cameraConstantBuffer.get() };
                filterBufferList = { engineConstantBuffer.get() };

                constantBufferDescription.stride = sizeof(LightConstantData);
                lightConstantBuffer = videoDevice->createBuffer(constantBufferDescription);
                lightConstantBuffer->setName("renderer:lightConstantBuffer");

                lightBufferList = { lightConstantBuffer.get() };

                static const char program[] =
                    "struct Output" \
                    "{" \
                    "    float4 screen : SV_POSITION;" \
                    "    float2 texCoord : TEXCOORD0;" \
                    "};" \
                    "" \
                    "Output mainVertexProgram(in uint vertexID : SV_VertexID)" \
                    "{" \
                    "    Output output;" \
                    "    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);" \
                    "    output.screen = float4(output.texCoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);" \
                    "    return output;" \
                    "}" \
                    "" \
                    "struct Input" \
                    "{" \
                    "    float4 screen : SV_POSITION;\r\n" \
                    "    float2 texCoord : TEXCOORD0;" \
                    "};" \
                    "" \
                    "Texture2D<float3> inputBuffer : register(t0);" \
                    "float3 mainPixelProgram(in Input input) : SV_TARGET0" \
                    "{" \
                    "    uint width, height, mipMapCount;" \
                    "    inputBuffer.GetDimensions(0, width, height, mipMapCount);" \
                    "    return inputBuffer[input.texCoord * float2(width, height)];" \
                    "}";

                auto compiledVertexProgram = resources->compileProgram(Video::PipelineType::Vertex, "deferredVertexProgram", "mainVertexProgram", program);
                deferredVertexProgram = videoDevice->createProgram(Video::PipelineType::Vertex, compiledVertexProgram.data(), compiledVertexProgram.size());
                deferredVertexProgram->setName("renderer:deferredVertexProgram");

                auto compiledPixelProgram = resources->compileProgram(Video::PipelineType::Pixel, "deferredPixelProgram", "mainPixelProgram", program);
                deferredPixelProgram = videoDevice->createProgram(Video::PipelineType::Pixel, compiledPixelProgram.data(), compiledPixelProgram.size());
                deferredPixelProgram->setName("renderer:deferredPixelProgram");

                Video::Buffer::Description lightBufferDescription;
                lightBufferDescription.type = Video::Buffer::Type::Structured;
                lightBufferDescription.flags = Video::Buffer::Flags::Mappable | Video::Buffer::Flags::Resource;

                Video::Buffer::Description tileBufferDescription;
                tileBufferDescription.type = Video::Buffer::Type::Raw;
                tileBufferDescription.flags = Video::Buffer::Flags::Mappable | Video::Buffer::Flags::Resource;
                tileBufferDescription.format = Video::Format::R32G32_UINT;
                tileBufferDescription.count = GridSize;
                tileOffsetCountBuffer = videoDevice->createBuffer(tileBufferDescription);
                tileOffsetCountBuffer->setName("renderer:tileOffsetCountBuffer");

                lightIndexList.reserve(GridSize * 10);
                tileBufferDescription.format = Video::Format::R16_UINT;
                tileBufferDescription.count = lightIndexList.capacity();
                lightIndexBuffer = videoDevice->createBuffer(tileBufferDescription);
                lightIndexBuffer->setName("renderer:lightIndexBuffer");
            }

            void initializeUI(void)
            {
                LockedWrite{ std::cout } << "Initializing user interface data";

                static char const vertexShader[] =
                    "cbuffer vertexBuffer : register(b0)" \
                    "{" \
                    "    float4x4 ProjectionMatrix;" \
                    "};" \
                    "" \
                    "struct VertexInput" \
                    "{" \
                    "    float2 position : POSITION;" \
                    "    float4 color : COLOR0;" \
                    "    float2 texCoord  : TEXCOORD0;" \
                    "};" \
                    "" \
                    "struct PixelOutput" \
                    "{" \
                    "    float4 position : SV_POSITION;" \
                    "    float4 color : COLOR0;" \
                    "    float2 texCoord  : TEXCOORD0;" \
                    "};" \
                    "" \
                    "PixelOutput main(in VertexInput input)" \
                    "{" \
                    "    PixelOutput output;" \
                    "    output.position = mul(ProjectionMatrix, float4(input.position.xy, 0.0f, 1.0f));" \
                    "    output.color = input.color;" \
                    "    output.texCoord  = input.texCoord;" \
                    "    return output;" \
                    "}";

                auto &compiled = resources->compileProgram(Video::PipelineType::Vertex, "uiVertexProgram", "main", vertexShader);
                gui.vertexProgram = videoDevice->createProgram(Video::PipelineType::Vertex, compiled.data(), compiled.size());
                gui.vertexProgram->setName("core:vertexProgram");

                std::vector<Video::InputElement> elementList;

                Video::InputElement element;
                element.format = Video::Format::R32G32_FLOAT;
                element.semantic = Video::InputElement::Semantic::Position;
                elementList.push_back(element);

                element.format = Video::Format::R32G32_FLOAT;
                element.semantic = Video::InputElement::Semantic::TexCoord;
                elementList.push_back(element);

                element.format = Video::Format::R8G8B8A8_UNORM;
                element.semantic = Video::InputElement::Semantic::Color;
                elementList.push_back(element);

                gui.inputLayout = videoDevice->createInputLayout(elementList, compiled.data(), compiled.size());
                gui.inputLayout->setName("core:inputLayout");

                Video::Buffer::Description constantBufferDescription;
                constantBufferDescription.stride = sizeof(Math::Float4x4);
                constantBufferDescription.count = 1;
                constantBufferDescription.type = Video::Buffer::Type::Constant;
                gui.constantBuffer = videoDevice->createBuffer(constantBufferDescription);
                gui.constantBuffer->setName("core:constantBuffer");

                static char const pixelShader[] =
                    "struct PixelInput" \
                    "{" \
                    "    float4 position : SV_POSITION;" \
                    "    float4 color : COLOR0;" \
                    "    float2 texCoord  : TEXCOORD0;" \
                    "};" \
                    "" \
                    "sampler uiSampler;" \
                    "Texture2D<float4> uiTexture : register(t0);" \
                    "" \
                    "float4 main(PixelInput input) : SV_Target" \
                    "{" \
                    "    return (input.color * uiTexture.Sample(uiSampler, input.texCoord));" \
                    "}";

                compiled = resources->compileProgram(Video::PipelineType::Pixel, "uiPixelProgram", "main", pixelShader);
                gui.pixelProgram = videoDevice->createProgram(Video::PipelineType::Pixel, compiled.data(), compiled.size());
                gui.pixelProgram->setName("core:pixelProgram");

                Video::BlendState::Description blendStateInformation;
                blendStateInformation[0].enable = true;
                blendStateInformation[0].colorSource = Video::BlendState::Source::SourceAlpha;
                blendStateInformation[0].colorDestination = Video::BlendState::Source::InverseSourceAlpha;
                blendStateInformation[0].colorOperation = Video::BlendState::Operation::Add;
                blendStateInformation[0].alphaSource = Video::BlendState::Source::InverseSourceAlpha;
                blendStateInformation[0].alphaDestination = Video::BlendState::Source::Zero;
                blendStateInformation[0].alphaOperation = Video::BlendState::Operation::Add;
                gui.blendState = videoDevice->createBlendState(blendStateInformation);
                gui.blendState->setName("core:blendState");

                Video::RenderState::Description renderStateInformation;
                renderStateInformation.fillMode = Video::RenderState::FillMode::Solid;
                renderStateInformation.cullMode = Video::RenderState::CullMode::None;
                renderStateInformation.scissorEnable = true;
                renderStateInformation.depthClipEnable = true;
                gui.renderState = videoDevice->createRenderState(renderStateInformation);
                gui.renderState->setName("core:renderState");

                Video::DepthState::Description depthStateInformation;
                depthStateInformation.enable = true;
                depthStateInformation.comparisonFunction = Video::ComparisonFunction::LessEqual;
                depthStateInformation.writeMask = Video::DepthState::Write::Zero;
                gui.depthState = videoDevice->createDepthState(depthStateInformation);
                gui.depthState->setName("core:depthState");

                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.Fonts->AddFontFromFileTTF(getContext()->getRootFileName("data", "fonts", "Ruda-Bold.ttf").u8string().c_str(), 14.0f);

                ImFontConfig fontConfig;
                fontConfig.MergeMode = true;

                fontConfig.GlyphOffset.y = 1.0f;
                const ImWchar fontAwesomeRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
                imGuiIo.Fonts->AddFontFromFileTTF(getContext()->getRootFileName("data", "fonts", "fontawesome-webfont.ttf").u8string().c_str(), 16.0f, &fontConfig, fontAwesomeRanges);

                fontConfig.GlyphOffset.y = 3.0f;
                const ImWchar googleIconRanges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
                imGuiIo.Fonts->AddFontFromFileTTF(getContext()->getRootFileName("data", "fonts", "MaterialIcons-Regular.ttf").u8string().c_str(), 16.0f, &fontConfig, googleIconRanges);

                imGuiIo.Fonts->Build();

                uint8_t *pixels = nullptr;
                int32_t fontWidth = 0, fontHeight = 0;
                imGuiIo.Fonts->GetTexDataAsRGBA32(&pixels, &fontWidth, &fontHeight);

                Video::Texture::Description fontDescription;
                fontDescription.format = Video::Format::R8G8B8A8_UNORM;
                fontDescription.width = fontWidth;
                fontDescription.height = fontHeight;
                fontDescription.flags = Video::Texture::Flags::Resource;
                gui.fontTexture = videoDevice->createTexture(fontDescription, pixels);

                imGuiIo.Fonts->TexID = static_cast<Video::Object *>(gui.fontTexture.get());

                imGuiIo.UserData = this;
                imGuiIo.RenderDrawListsFn = [](ImDrawData *drawData)
                {
                    ImGuiIO &imGuiIo = ImGui::GetIO();
                    Renderer *renderer = static_cast<Renderer *>(imGuiIo.UserData);
                    renderer->renderUI(drawData);
                };

                ImGui::ResetStyle(ImGuiStyle_OSXInverse);
                auto &style = ImGui::GetStyle();
                style.WindowPadding.x = style.WindowPadding.y;
                style.FramePadding.x = style.FramePadding.y;
            }

            ~Renderer(void)
            {
                workerPool.drain();

                population->onReset.disconnect(this, &Renderer::onReset);
                population->onEntityCreated.disconnect(this, &Renderer::onEntityCreated);
                population->onEntityDestroyed.disconnect(this, &Renderer::onEntityDestroyed);
                population->onComponentAdded.disconnect(this, &Renderer::onComponentAdded);
                population->onComponentRemoved.disconnect(this, &Renderer::onComponentRemoved);
                population->onUpdate[1000].disconnect(this, &Renderer::onUpdate);

                ImGui::GetIO().Fonts->TexID = 0;
                ImGui::Shutdown();
            }

            void addEntity(Plugin::Entity * const entity)
            {
                if (entity->hasComponents<Components::Transform, Components::Color>())
                {
                    directionalLightData.addEntity(entity);
                    pointLightData.addEntity(entity);
                    spotLightData.addEntity(entity);
                }
            }

            void removeEntity(Plugin::Entity * const entity)
            {
                directionalLightData.removeEntity(entity);
                pointLightData.removeEntity(entity);
                spotLightData.removeEntity(entity);
            }

            // ImGui
            std::vector<Math::UInt4> scissorBoxList = std::vector<Math::UInt4>(1);
            std::vector<Video::Object *> textureList = std::vector<Video::Object *>(1);
            void renderUI(ImDrawData *drawData)
            {
                if (!gui.vertexBuffer || gui.vertexBuffer->getDescription().count < uint32_t(drawData->TotalVtxCount))
                {
                    Video::Buffer::Description vertexBufferDescription;
                    vertexBufferDescription.stride = sizeof(ImDrawVert);
                    vertexBufferDescription.count = drawData->TotalVtxCount;
                    vertexBufferDescription.type = Video::Buffer::Type::Vertex;
                    vertexBufferDescription.flags = Video::Buffer::Flags::Mappable;
                    gui.vertexBuffer = videoDevice->createBuffer(vertexBufferDescription);
                    gui.vertexBuffer->setName(String::Format("core:vertexBuffer:%v", gui.vertexBuffer.get()));
                }

                if (!gui.indexBuffer || gui.indexBuffer->getDescription().count < uint32_t(drawData->TotalIdxCount))
                {
                    Video::Buffer::Description vertexBufferDescription;
                    vertexBufferDescription.count = drawData->TotalIdxCount;
                    vertexBufferDescription.type = Video::Buffer::Type::Index;
                    vertexBufferDescription.flags = Video::Buffer::Flags::Mappable;
                    switch (sizeof(ImDrawIdx))
                    {
                    case 2:
                        vertexBufferDescription.format = Video::Format::R16_UINT;
                        break;

                    case 4:
                        vertexBufferDescription.format = Video::Format::R32_UINT;
                        break;
                    };

                    gui.indexBuffer = videoDevice->createBuffer(vertexBufferDescription);
                    gui.indexBuffer->setName(String::Format("core:vertexBuffer:%v", gui.indexBuffer.get()));
                }

                bool dataUploaded = false;
                ImDrawVert* vertexData = nullptr;
                ImDrawIdx* indexData = nullptr;
                if (videoDevice->mapBuffer(gui.vertexBuffer.get(), vertexData))
                {
                    if (videoDevice->mapBuffer(gui.indexBuffer.get(), indexData))
                    {
                        for (int32_t commandListIndex = 0; commandListIndex < drawData->CmdListsCount; ++commandListIndex)
                        {
                            const ImDrawList* commandList = drawData->CmdLists[commandListIndex];
                            std::copy(commandList->VtxBuffer.Data, (commandList->VtxBuffer.Data + commandList->VtxBuffer.Size), vertexData);
                            std::copy(commandList->IdxBuffer.Data, (commandList->IdxBuffer.Data + commandList->IdxBuffer.Size), indexData);
                            vertexData += commandList->VtxBuffer.Size;
                            indexData += commandList->IdxBuffer.Size;
                        }

                        dataUploaded = true;
                        videoDevice->unmapBuffer(gui.indexBuffer.get());
                    }

                    videoDevice->unmapBuffer(gui.vertexBuffer.get());
                }

                if (dataUploaded)
                {
                    auto backBuffer = videoDevice->getBackBuffer();
                    uint32_t width = backBuffer->getDescription().width;
                    uint32_t height = backBuffer->getDescription().height;
                    auto orthographic = Math::Float4x4::MakeOrthographic(0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f);
                    videoDevice->updateResource(gui.constantBuffer.get(), &orthographic);

                    auto videoContext = videoDevice->getDefaultContext();
                    videoContext->setRenderTargetList({ videoDevice->getBackBuffer() }, nullptr);
                    videoContext->setViewportList({ Video::ViewPort(Math::Float2::Zero, Math::Float2(width, height), 0.0f, 1.0f) });

                    videoContext->setInputLayout(gui.inputLayout.get());
                    videoContext->setVertexBufferList({ gui.vertexBuffer.get() }, 0);
                    videoContext->setIndexBuffer(gui.indexBuffer.get(), 0);
                    videoContext->setPrimitiveType(Video::PrimitiveType::TriangleList);
                    videoContext->vertexPipeline()->setProgram(gui.vertexProgram.get());
                    videoContext->pixelPipeline()->setProgram(gui.pixelProgram.get());
                    videoContext->vertexPipeline()->setConstantBufferList({ gui.constantBuffer.get() }, 0);
                    videoContext->pixelPipeline()->setSamplerStateList({ bufferSamplerState.get() }, 0);

                    videoContext->setBlendState(gui.blendState.get(), Math::Float4::Black, 0xFFFFFFFF);
                    videoContext->setDepthState(gui.depthState.get(), 0);
                    videoContext->setRenderState(gui.renderState.get());

                    uint32_t vertexOffset = 0;
                    uint32_t indexOffset = 0;
                    for (int32_t commandListIndex = 0; commandListIndex < drawData->CmdListsCount; ++commandListIndex)
                    {
                        const ImDrawList* commandList = drawData->CmdLists[commandListIndex];
                        for (int32_t commandIndex = 0; commandIndex < commandList->CmdBuffer.Size; ++commandIndex)
                        {
                            const ImDrawCmd* command = &commandList->CmdBuffer[commandIndex];
                            if (command->UserCallback)
                            {
                                command->UserCallback(commandList, command);
                            }
                            else
                            {
                                scissorBoxList[0].minimum.x = uint32_t(command->ClipRect.x);
                                scissorBoxList[0].minimum.y = uint32_t(command->ClipRect.y);
                                scissorBoxList[0].maximum.x = uint32_t(command->ClipRect.z);
                                scissorBoxList[0].maximum.y = uint32_t(command->ClipRect.w);
                                videoContext->setScissorList(scissorBoxList);

                                textureList[0] = reinterpret_cast<Video::Object *>(command->TextureId);
                                videoContext->pixelPipeline()->setResourceList(textureList, 0);

                                videoContext->drawIndexedPrimitive(command->ElemCount, indexOffset, vertexOffset);
                            }

                            indexOffset += command->ElemCount;
                        }

                        vertexOffset += commandList->VtxBuffer.Size;
                    }
                }
            }

            // Clustered Lighting
            inline Math::Float3 getLightDirection(Math::Quaternion const &quaternion) const
            {
                const float xx(quaternion.x * quaternion.x);
                const float yy(quaternion.y * quaternion.y);
                const float zz(quaternion.z * quaternion.z);
                const float ww(quaternion.w * quaternion.w);
                const float length(xx + yy + zz + ww);
                if (length == 0.0f)
                {
                    return Math::Float3(0.0f, 1.0f, 0.0f);
                }
                else
                {
                    const float determinant(1.0f / length);
                    const float xy(quaternion.x * quaternion.y);
                    const float xw(quaternion.x * quaternion.w);
                    const float yz(quaternion.y * quaternion.z);
                    const float zw(quaternion.z * quaternion.w);
                    return -Math::Float3((2.0f * (xy - zw) * determinant), ((-xx + yy - zz + ww) * determinant), (2.0f * (yz + xw) * determinant));
                }
            }

            inline void updateClipRegionRoot(float tangentCoordinate, float lightCoordinate, float lightDepth, float radius, float radiusSquared, float lightRangeSquared, float cameraScale, float& minimum, float& maximum) const
            {
                const float nz = ((radius - tangentCoordinate * lightCoordinate) / lightDepth);
                const float pz = ((lightRangeSquared - radiusSquared) / (lightDepth - (nz / tangentCoordinate) * lightCoordinate));
                if (pz > 0.0f)
                {
                    const float clip = (-nz * cameraScale / tangentCoordinate);
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

            inline void updateClipRegion(float lightCoordinate, float lightDepth, float radius, float cameraScale, float& minimum, float& maximum) const
            {
                const float radiusSquared = (radius * radius);
                const float lightDepthSquared = (lightDepth * lightDepth);
                const float lightCoordinateSquared = (lightCoordinate * lightCoordinate);
                const float lightRangeSquared = (lightCoordinateSquared + lightDepthSquared);
                const float distanceSquared = ((radiusSquared * lightCoordinateSquared) - (lightRangeSquared * (radiusSquared - lightDepthSquared)));
                if (distanceSquared > 0.0f)
                {
                    const float projectedRadius = (radius * lightCoordinate);
                    const float distance = std::sqrt(distanceSquared);
                    const float positiveTangent = ((projectedRadius + distance) / lightRangeSquared);
                    const float negativeTangent = ((projectedRadius - distance) / lightRangeSquared);
                    updateClipRegionRoot(positiveTangent, lightCoordinate, lightDepth, radius, radiusSquared, lightRangeSquared, cameraScale, minimum, maximum);
                    updateClipRegionRoot(negativeTangent, lightCoordinate, lightDepth, radius, radiusSquared, lightRangeSquared, cameraScale, minimum, maximum);
                }
            }

            // Returns bounding box [min.xy, max.xy] in clip [-1, 1] space.
            inline Math::Float4 getClipBounds(Math::Float3 const &position, float range) const
            {
                // Early out with empty rectangle if the light is too far behind the view frustum
                Math::Float4 clipRegion(1.0f, 1.0f, 0.0f, 0.0f);
                if ((position.z + range) >= currentCamera.nearClip)
                {
                    clipRegion.set(-1.0f, -1.0f, 1.0f, 1.0f);
                    updateClipRegion(position.x, position.z, range, currentCamera.projectionMatrix.rx.x, clipRegion.minimum.x, clipRegion.maximum.x);
                    updateClipRegion(position.y, position.z, range, currentCamera.projectionMatrix.ry.y, clipRegion.minimum.y, clipRegion.maximum.y);
                }

                return clipRegion;
            }

            inline Math::Float4 getScreenBounds(Math::Float3 const &position, float range) const
            {
                const auto clipBounds((getClipBounds(position, range) + 1.0f) * 0.5f);
                return Math::Float4(clipBounds.x, (1.0f - clipBounds.w), clipBounds.z, (1.0f - clipBounds.y));
            }

            bool isSeparated(float x, float y, float z, Math::Float3 const &position, float range) const
            {
                // sub-frustrum bounds in view space       
                const float depthScale = (1.0f / GridDepth * (currentCamera.farClip - currentCamera.nearClip) + currentCamera.nearClip);
                const float minimumZ = (z * depthScale);
                const float maximumZ = ((z + 1.0f) * depthScale);

                const Math::Float4 tileBounds(x, (x + 1.0f), y, (y + 1.0f));
                const Math::Float4 projectionScale(Math::Float2(currentCamera.projectionMatrix.rx.x), Math::Float2(currentCamera.projectionMatrix.ry.y));
                const auto gridScale = (Negate * (1.0f - 2.0f / GridDimensions * tileBounds));
                const auto minimum = (gridScale * minimumZ / projectionScale);
                const auto maximum = (gridScale * maximumZ / projectionScale);

                // heuristic plane separation test - works pretty well in practice
                const Math::Float3 minimumZcenter(((minimum.x + minimum.y) * 0.5f), ((minimum.z + minimum.w) * 0.5f), minimumZ);
                const Math::Float3 maximumZcenter(((maximum.x + maximum.y) * 0.5f), ((maximum.z + maximum.w) * 0.5f), maximumZ);
                const Math::Float3 center((minimumZcenter + maximumZcenter) * 0.5f);
                const Math::Float3 normal((center - position).getNormal());

                // compute distance of all corners to the tangent plane, with a few shortcuts (saves 14 muls)
                Math::Float2 tileCorners(-normal.dot(position));
                tileCorners.minimum += std::min((normal.x * minimum.x), (normal.x * minimum.y));
                tileCorners.minimum += std::min((normal.y * minimum.z), (normal.y * minimum.w));
                tileCorners.minimum += (normal.z * minimumZ);
                tileCorners.maximum += std::min((normal.x * maximum.x), (normal.x * maximum.y));
                tileCorners.maximum += std::min((normal.y * maximum.z), (normal.y * maximum.w));
                tileCorners.maximum += (normal.z * maximumZ);
                return (std::min(tileCorners.minimum, tileCorners.maximum) > range);
            }

            void addLightCluster(Math::Float3 const &position, float range, uint32_t lightIndex, bool pointLight)
            {
                const Math::Float4 screenBounds(getScreenBounds(position, range));
                const Math::Int4 gridBounds(
                    std::max(0, int32_t(std::floor(screenBounds.x * GridWidth))),
                    std::max(0, int32_t(std::floor(screenBounds.y * GridHeight))),
                    std::min(int32_t(std::ceil(screenBounds.z * GridWidth)), GridWidth),
                    std::min(int32_t(std::ceil(screenBounds.w * GridHeight)), GridHeight)
                );

                const float centerDepth = ((position.z - currentCamera.nearClip) / (currentCamera.farClip - currentCamera.nearClip));
                const float rangeDepth = (range / (currentCamera.farClip - currentCamera.nearClip));
                const Math::Int2 depthBounds(
                    std::max(0, int32_t(std::floor((centerDepth - rangeDepth) * GridDepth))),
                    std::min(int32_t(std::ceil((centerDepth + rangeDepth) * GridDepth)), GridDepth)
                );

                concurrency::parallel_for(depthBounds.minimum, depthBounds.maximum, [&](auto z) -> void
                {
                    const uint32_t zSlice = (z * GridHeight);
                    for (auto y = gridBounds.minimum.y; y < gridBounds.maximum.y; ++y)
                    {
                        const uint32_t ySlize = ((zSlice + y) * GridWidth);
                        for (auto x = gridBounds.minimum.x; x < gridBounds.maximum.x; ++x)
                        {
                            if (!isSeparated(float(x), float(y), float(z), position, range))
                            {
                                const uint32_t gridIndex = (ySlize + x);
                                auto &gridData = tileLightIndexList[gridIndex];
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

            void addLight(Plugin::Entity * const entity, const Components::PointLight &lightComponent)
            {
                auto const &transformComponent = entity->getComponent<Components::Transform>();
                auto const &colorComponent = entity->getComponent<Components::Color>();

                auto lightIterator = pointLightData.lightList.grow_by(1);
                PointLightData &lightData = (*lightIterator);
                lightData.radiance = (colorComponent.value.xyz * lightComponent.intensity);
                lightData.position = currentCamera.viewMatrix.transform(transformComponent.position);
                lightData.radius = lightComponent.radius;
                lightData.range = lightComponent.range;

                const auto lightIndex = std::distance(std::begin(pointLightData.lightList), lightIterator);
                addLightCluster(lightData.position, (lightData.radius + lightData.range), lightIndex, true);
            }

            void addLight(Plugin::Entity * const entity, const Components::SpotLight &lightComponent)
            {
                auto const &transformComponent = entity->getComponent<Components::Transform>();
                auto const &colorComponent = entity->getComponent<Components::Color>();

                auto lightIterator = spotLightData.lightList.grow_by(1);
                SpotLightData &lightData = (*lightIterator);
                lightData.radiance = (colorComponent.value.xyz * lightComponent.intensity);
                lightData.position = currentCamera.viewMatrix.transform(transformComponent.position);
                lightData.radius = lightComponent.radius;
                lightData.range = lightComponent.range;
                lightData.direction = currentCamera.viewMatrix.rotate(getLightDirection(transformComponent.rotation));
                lightData.innerAngle = lightComponent.innerAngle;
                lightData.outerAngle = lightComponent.outerAngle;
                lightData.coneFalloff = lightComponent.coneFalloff;

                const auto lightIndex = std::distance(std::begin(spotLightData.lightList), lightIterator);
                addLightCluster(lightData.position, (lightData.radius + lightData.range), lightIndex, false);
            }

            // Plugin::Population Slots
            void onReset(void)
            {
                directionalLightData.clearEntities();
                pointLightData.clearEntities();
                spotLightData.clearEntities();
            }

            void onEntityCreated(Plugin::Entity * const entity)
            {
                addEntity(entity);
            }

            void onEntityDestroyed(Plugin::Entity * const entity)
            {
                removeEntity(entity);
            }

            void onComponentAdded(Plugin::Entity * const entity)
            {
                addEntity(entity);
            }

            void onComponentRemoved(Plugin::Entity * const entity)
            {
                removeEntity(entity);
            }

            // Renderer
            void queueCamera(Math::Float4x4 const &viewMatrix, Math::Float4x4 const &projectionMatrix, float nearClip, float farClip, std::string const *name, ResourceHandle cameraTarget, std::string const &forceShader)
            {
                Camera renderCall;
                renderCall.viewMatrix = viewMatrix;
                renderCall.projectionMatrix = projectionMatrix;
                renderCall.viewFrustum.create(viewMatrix * projectionMatrix);
                renderCall.nearClip = nearClip;
                renderCall.farClip = farClip;
                renderCall.cameraTarget = cameraTarget;
                renderCall.name = (name ? *name : String::Format("camera_%v", cameraQueue.unsafe_size()));
                if (!forceShader.empty())
                {
                    renderCall.forceShader = resources->getShader(forceShader);
                }

                cameraQueue.push(renderCall);
            }

            void queueDrawCall(VisualHandle plugin, MaterialHandle material, std::function<void(Video::Device::Context *videoContext)> &&draw)
            {
                if (plugin && material && draw)
                {
                    ShaderHandle shader = (currentCamera.forceShader ? currentCamera.forceShader : resources->getMaterialShader(material));
                    if (shader)
                    {
                        drawCallList.push_back(DrawCallValue(material, plugin, shader, std::move(draw)));
                    }
                }
            }

            // Plugin::Core Slots
            void onUpdate(float frameTime)
            {
                assert(videoDevice);
                assert(population);

                profiler->beginFrame();
                EngineConstantData engineConstantData;
                engineConstantData.frameTime = frameTime;
                engineConstantData.worldTime = 0.0f;
                videoDevice->updateResource(engineConstantBuffer.get(), &engineConstantData);
                Video::Device::Context *videoContext = videoDevice->getDefaultContext();
                while (cameraQueue.try_pop(currentCamera))
                {
                    profiler->timeStamp(String::Format("Begin Camera: %v", currentCamera.name), Profiler::Flags::PostIndent);

                    drawCallList.clear();
                    onQueueDrawCalls(currentCamera.viewFrustum, currentCamera.viewMatrix, currentCamera.projectionMatrix);
                    if (!drawCallList.empty())
                    {
                        const auto backBuffer = videoDevice->getBackBuffer();
                        const auto width = backBuffer->getDescription().width;
                        const auto height = backBuffer->getDescription().height;

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

                            isLightingRequired |= shader->isLightingRequired();
                            auto &shaderList = drawCallSetMap[shader->getDrawOrder()];
                            shaderList.push_back(DrawCallSet(shader, beginShaderList, endShaderList));
                        }

                        if (isLightingRequired)
                        {
                            auto directionalLightsDone = workerPool.enqueue([&](void) -> void
                            {
                                directionalLightData.lightList.clear();
                                directionalLightData.lightList.reserve(directionalLightData.entityList.size());
                                std::for_each(std::begin(directionalLightData.entityList), std::end(directionalLightData.entityList), [&](Plugin::Entity * const entity) -> void
                                {
                                    auto const &transformComponent = entity->getComponent<Components::Transform>();
                                    auto const &colorComponent = entity->getComponent<Components::Color>();
                                    auto const &lightComponent = entity->getComponent<Components::DirectionalLight>();

                                    DirectionalLightData lightData;
                                    lightData.radiance = (colorComponent.value.xyz * lightComponent.intensity);
                                    lightData.direction = currentCamera.viewMatrix.rotate(getLightDirection(transformComponent.rotation));
                                    directionalLightData.lightList.push_back(lightData);
                                });

                                directionalLightData.createBuffer();
                            });

                            auto frustum = Math::SIMD::loadFrustum((Math::Float4 *)currentCamera.viewFrustum.planeList);

                            lightIndexCount = 0;
                            concurrency::parallel_for_each(std::begin(tileLightIndexList), std::end(tileLightIndexList), [&](auto &gridData) -> void
                            {
                                gridData.pointLightList.clear();
                                gridData.spotLightList.clear();
                            });

                            auto pointLightsDone = workerPool.enqueue([&](void) -> void
                            {
                                pointLightData.update(videoDevice, frustum, [this](Plugin::Entity * const entity, const Components::PointLight &lightComponent) -> void
                                {
                                    addLight(entity, lightComponent);
                                });
                            });

                            auto spotLightsDone = workerPool.enqueue([&](void) -> void
                            {
                                spotLightData.update(videoDevice, frustum, [this](Plugin::Entity * const entity, const Components::SpotLight &lightComponent) -> void
                                {
                                    addLight(entity, lightComponent);
                                });
                            });

                            directionalLightsDone.get();
                            pointLightsDone.get();
                            spotLightsDone.get();

                            lightIndexList.clear();
                            lightIndexList.reserve(lightIndexCount);
                            for (uint32_t tileIndex = 0; tileIndex < GridSize; ++tileIndex)
                            {
                                auto &tileOffsetCount = tileOffsetCountList[tileIndex];
                                auto const &tileLightIndex = tileLightIndexList[tileIndex];
                                tileOffsetCount.indexOffset = lightIndexList.size();
                                tileOffsetCount.pointLightCount = uint16_t(tileLightIndex.pointLightList.size() & 0xFFFF);
                                tileOffsetCount.spotLightCount = uint16_t(tileLightIndex.spotLightList.size() & 0xFFFF);
                                lightIndexList.insert(std::end(lightIndexList), std::begin(tileLightIndex.pointLightList), std::end(tileLightIndex.pointLightList));
                                lightIndexList.insert(std::end(lightIndexList), std::begin(tileLightIndex.spotLightList), std::end(tileLightIndex.spotLightList));
                            }

                            if (!directionalLightData.updateBuffer() ||
                                !pointLightData.updateBuffer() ||
                                !spotLightData.updateBuffer())
                            {
                                continue;
                            }

                            TileOffsetCount *tileOffsetCountData = nullptr;
                            if (videoDevice->mapBuffer(tileOffsetCountBuffer.get(), tileOffsetCountData))
                            {
                                std::copy(std::begin(tileOffsetCountList), std::end(tileOffsetCountList), tileOffsetCountData);
                                videoDevice->unmapBuffer(tileOffsetCountBuffer.get());
                            }
                            else
                            {
                                continue;
                            }

                            if (!lightIndexList.empty())
                            {
                                if (!lightIndexBuffer || lightIndexBuffer->getDescription().count < lightIndexList.size())
                                {
                                    lightIndexBuffer = nullptr;

                                    Video::Buffer::Description tileBufferDescription;
                                    tileBufferDescription.type = Video::Buffer::Type::Raw;
                                    tileBufferDescription.flags = Video::Buffer::Flags::Mappable | Video::Buffer::Flags::Resource;
                                    tileBufferDescription.format = Video::Format::R16_UINT;
                                    tileBufferDescription.count = lightIndexList.size();
                                    lightIndexBuffer = videoDevice->createBuffer(tileBufferDescription);
                                    lightIndexBuffer->setName(String::Format("renderer:lightIndexBuffer:%v", lightIndexBuffer.get()));
                                }

                                uint16_t *lightIndexData = nullptr;
                                if (videoDevice->mapBuffer(lightIndexBuffer.get(), lightIndexData))
                                {
                                    std::copy(std::begin(lightIndexList), std::end(lightIndexList), lightIndexData);
                                    videoDevice->unmapBuffer(lightIndexBuffer.get());
                                }
                                else
                                {
                                    continue;
                                }
                            }

                            LightConstantData lightConstants;
                            lightConstants.directionalLightCount = directionalLightData.lightList.size();
                            lightConstants.pointLightCount = pointLightData.lightList.size();
                            lightConstants.spotLightCount = spotLightData.lightList.size();
                            lightConstants.gridSize.x = GridWidth;
                            lightConstants.gridSize.y = GridHeight;
                            lightConstants.gridSize.z = GridDepth;
                            lightConstants.tileSize.x = (width / GridWidth);
                            lightConstants.tileSize.y = (height / GridHeight);
                            videoDevice->updateResource(lightConstantBuffer.get(), &lightConstants);
                        }

                        CameraConstantData cameraConstantData;
                        cameraConstantData.fieldOfView.x = (1.0f / currentCamera.projectionMatrix._11);
                        cameraConstantData.fieldOfView.y = (1.0f / currentCamera.projectionMatrix._22);
                        cameraConstantData.nearClip = currentCamera.nearClip;
                        cameraConstantData.farClip = currentCamera.farClip;
                        cameraConstantData.viewMatrix = currentCamera.viewMatrix;
                        cameraConstantData.projectionMatrix = currentCamera.projectionMatrix;
                        videoDevice->updateResource(cameraConstantBuffer.get(), &cameraConstantData);

                        videoContext->clearState();
                        videoContext->geometryPipeline()->setConstantBufferList(shaderBufferList, 0);
                        videoContext->vertexPipeline()->setConstantBufferList(shaderBufferList, 0);
                        videoContext->pixelPipeline()->setConstantBufferList(shaderBufferList, 0);
                        videoContext->computePipeline()->setConstantBufferList(shaderBufferList, 0);

                        videoContext->pixelPipeline()->setSamplerStateList(samplerList, 0);

                        videoContext->setPrimitiveType(Video::PrimitiveType::TriangleList);

                        if (isLightingRequired)
                        {
                            lightResoruceList =
                            {
                                directionalLightData.lightDataBuffer.get(),
                                pointLightData.lightDataBuffer.get(),
                                spotLightData.lightDataBuffer.get(),
                                tileOffsetCountBuffer.get(),
                                lightIndexBuffer.get()
                            };

                            videoContext->pixelPipeline()->setConstantBufferList(lightBufferList, 3);
                            videoContext->pixelPipeline()->setResourceList(lightResoruceList, 0);
                        }

                        profiler->timeStamp("Buffer Management");

                        uint8_t shaderIndex = 0;
                        std::string finalOutput;
                        auto forceShader = (currentCamera.forceShader ? resources->getShader(currentCamera.forceShader) : nullptr);
                        for (auto const &shaderDrawCallList : drawCallSetMap)
                        {
                            for (auto const &shaderDrawCall : shaderDrawCallList.second)
                            {
                                auto &shader = shaderDrawCall.shader;
                                profiler->timeStamp(String::Format("Begin Shader: %v", shader->getName()), Profiler::Flags::PostIndent);

                                finalOutput = shader->getOutput();

                                for (auto pass = shader->begin(videoContext, cameraConstantData.viewMatrix, currentCamera.viewFrustum); pass; pass = pass->next())
                                {
                                    resources->startResourceBlock();
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
                                                    resources->setMaterial(videoContext, pass.get(), currentMaterial, (forceShader == shader));
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
                                    profiler->timeStamp(String::Format("%v##%v", pass->getName(), shader->getName()));
                                }

                                profiler->timeStamp(String::Format("End Shader: %v", shader->getName()), Profiler::Flags::Hide | Profiler::Flags::PreUnindent);
                            }
                        }

                        videoContext->geometryPipeline()->clearConstantBufferList(2, 0);
                        videoContext->vertexPipeline()->clearConstantBufferList(2, 0);
                        videoContext->pixelPipeline()->clearConstantBufferList(2, 0);
                        videoContext->computePipeline()->clearConstantBufferList(2, 0);

                        if (currentCamera.cameraTarget)
                        {
                            auto finalHandle = resources->getResourceHandle(finalOutput);
                            renderOverlay(videoDevice->getDefaultContext(), finalHandle, currentCamera.cameraTarget);
                        }
                        else
                        {
                            screenOutput = finalOutput;
                        }
                    }

                    profiler->timeStamp(String::Format("End Camera: %v", currentCamera.name), Profiler::Flags::Hide | Profiler::Flags::PreUnindent);
                };

                auto screenHandle = resources->getResourceHandle(screenOutput);
                if (screenHandle)
                {
                    videoContext->clearState();
                    videoContext->geometryPipeline()->setConstantBufferList(filterBufferList, 0);
                    videoContext->vertexPipeline()->setConstantBufferList(filterBufferList, 0);
                    videoContext->pixelPipeline()->setConstantBufferList(filterBufferList, 0);
                    videoContext->computePipeline()->setConstantBufferList(filterBufferList, 0);

                    videoContext->pixelPipeline()->setSamplerStateList(samplerList, 0);

                    videoContext->setPrimitiveType(Video::PrimitiveType::TriangleList);

                    uint8_t filterIndex = 0;
                    videoContext->vertexPipeline()->setProgram(deferredVertexProgram.get());
                    for (auto const &filterName : { "tonemap" })
                    {
                        auto const filter = resources->getFilter(filterName);
                        if (filter)
                        {
                            profiler->timeStamp(String::Format("Begin Filter: %v", filter->getName()), Profiler::Flags::PostIndent);
                            for (auto pass = filter->begin(videoContext, screenHandle, ResourceHandle()); pass; pass = pass->next())
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
                                profiler->timeStamp(String::Format("%v##%v", pass->getName(), filter->getName()));
                            }

                            profiler->timeStamp(String::Format("End Filter: %v", filter->getName()), Profiler::Flags::Hide | Profiler::Flags::PreUnindent);
                        }
                    }

                    videoContext->geometryPipeline()->clearConstantBufferList(1, 0);
                    videoContext->vertexPipeline()->clearConstantBufferList(1, 0);
                    videoContext->pixelPipeline()->clearConstantBufferList(1, 0);
                    videoContext->computePipeline()->clearConstantBufferList(1, 0);
                }
                else
                {
                    static const JSON::Object Black = JSON::Array({ 0.0f, 0.0f, 0.0f, 1.0f });
                    auto blackPattern = resources->createPattern("color", Black);
                    renderOverlay(videoContext, blackPattern, ResourceHandle());
                }

                ImGuiIO &imGuiIo = ImGui::GetIO();
                imGuiIo.DeltaTime = frameTime;

                auto backBuffer = videoDevice->getBackBuffer();
                uint32_t width = backBuffer->getDescription().width;
                uint32_t height = backBuffer->getDescription().height;
                imGuiIo.DisplaySize = ImVec2(float(width), float(height));

                ImGui::NewFrame();
                onShowUserInterface(ImGui::GetCurrentContext());

                auto mainMenu = ImGui::FindWindowByName("##MainMenuBar");
                auto mainMenuShowing = (mainMenu ? mainMenu->Active : false);
                if (mainMenuShowing)
                {
                    ImGui::BeginMainMenuBar();
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(5.0f, 10.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5.0f, 10.0f));
                    if (ImGui::BeginMenu("Debug"))
                    {
                        ImGui::MenuItem("Information", "CTRL+I", &showDebugInformation);
                        ImGui::EndMenu();
                    }

                    ImGui::PopStyleVar(2);
                    ImGui::EndMainMenuBar();
                }

                int currentEventFrame = profiler->updateEventData();
                if (showDebugInformation)
                {
                    if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        UI::TextFrame("Post Process", ImVec2(ImGui::GetWindowContentRegionWidth(), 0.0f));
                        profiler->showEventData(currentEventFrame);

                        ImGui::End();
                    }
                }

                ImGui::Render();
                profiler->timeStamp("User Interface");

                videoDevice->present(true);
                profiler->endFrame();
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
                resources->setRenderTargetList(videoContext, { target }, nullptr);

                resources->drawPrimitive(videoContext, 3, 0);

                resources->clearRenderTargetList(videoContext, 1, false);
                resources->clearResourceList(videoContext->pixelPipeline(), 1, 0);
            }
        };

        GEK_REGISTER_CONTEXT_USER(Renderer);
    }; // namespace Implementation
}; // namespace Gek
