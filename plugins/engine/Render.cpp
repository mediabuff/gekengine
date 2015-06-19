﻿#include "GEK\Engine\RenderInterface.h"
#include "GEK\Engine\PluginInterface.h"
#include "GEK\Engine\ShaderInterface.h"
#include "GEK\Engine\MaterialInterface.h"
#include "GEK\Engine\PopulationInterface.h"
#include "GEK\Engine\ComponentInterface.h"
#include "GEK\Components\Transform.h"
#include "GEK\Components\Camera.h"
#include "GEK\Components\Light.h"
#include "GEK\Components\Color.h"
#include "GEK\Context\Common.h"
#include "GEK\Context\BaseUser.h"
#include "GEK\Context\BaseObservable.h"
#include "GEK\Utility\String.h"
#include "GEK\Utility\XML.h"
#include "GEK\Shape\Sphere.h"
#include <set>
#include <concurrent_vector.h>
#include <concurrent_unordered_map.h>
#include <ppl.h>

namespace std
{
    template <>
    struct hash<LPCSTR>
    {
        size_t operator()(const LPCSTR &value) const
        {
            return hash<string>()(string(value));
        }
    };

    template <>
    struct hash<LPCWSTR>
    {
        size_t operator()(const LPCWSTR &value) const
        {
            return hash<wstring>()(wstring(value));
        }
    };

    inline size_t hashCombine(const size_t upper, const size_t lower)
    {
        return upper ^ (lower + 0x9e3779b9 + (upper << 6) + (upper >> 2));
    }

    inline size_t hashCombine(void)
    {
        return 0;
    }

    template <typename T, typename... Ts>
    size_t hashCombine(const T& t, const Ts&... ts)
    {
        size_t seed = hash<T>()(t);
        if (sizeof...(ts) == 0)
        {
            return seed;
        }

        size_t remainder = hashCombine(ts...);   // not recursion!
        return hashCombine(seed, remainder);
    }
};

namespace Gek
{
    namespace Engine
    {
        namespace Render
        {
            class System : public Context::BaseUser
                , public BaseObservable
                , public Population::Observer
                , public Render::Interface
            {
            public:
                struct CameraConstantBuffer
                {
                    Math::Float2 fieldOfView;
                    float minimumDistance;
                    float maximumDistance;
                    Math::Float4x4 viewMatrix;
                    Math::Float4x4 projectionMatrix;
                    Math::Float4x4 inverseProjectionMatrix;
                    Math::Float4x4 transformMatrix;
                };

                struct LightingConstantBuffer
                {
                    UINT32 count;
                    UINT32 padding[3];
                };

                struct Light
                {
                    Math::Float3 position;
                    float distance;
                    float range;
                    Math::Float3 color;
                };

                struct DrawCommand
                {
                    LPCVOID instanceData;
                    UINT32 instanceStride;
                    UINT32 instanceCount;
                    CComPtr<Video3D::BufferInterface> vertexBuffer;
                    UINT32 vertexCount;
                    UINT32 firstVertex;
                    CComPtr<Video3D::BufferInterface> indexBuffer;
                    UINT32 indexCount;
                    UINT32 firstIndex;

                    DrawCommand(Video3D::BufferInterface *vertexBuffer, UINT32 vertexCount, UINT32 firstVertex)
                        : vertexBuffer(vertexBuffer)
                        , vertexCount(vertexCount)
                        , firstVertex(firstVertex)
                        , instanceData(nullptr)
                        , instanceStride(0)
                        , instanceCount(0)
                        , indexCount(0)
                        , firstIndex(0)
                    {
                    }

                    DrawCommand(Video3D::BufferInterface *vertexBuffer, UINT32 firstVertex, Video3D::BufferInterface *indexBuffer, UINT32 indexCount, UINT32 firstIndex)
                        : vertexBuffer(vertexBuffer)
                        , firstVertex(firstVertex)
                        , indexBuffer(indexBuffer)
                        , indexCount(indexCount)
                        , firstIndex(firstIndex)
                        , instanceData(nullptr)
                        , instanceStride(0)
                        , instanceCount(0)
                        , vertexCount(0)
                    {
                    }

                    DrawCommand(LPCVOID instanceData, UINT32 instanceStride, UINT32 instanceCount, Video3D::BufferInterface *vertexBuffer, UINT32 vertexCount, UINT32 firstVertex)
                        : instanceData(instanceData)
                        , instanceStride(instanceStride)
                        , instanceCount(instanceCount)
                        , vertexBuffer(vertexBuffer)
                        , vertexCount(vertexCount)
                        , firstVertex(firstVertex)
                        , indexCount(0)
                        , firstIndex(0)
                    {
                    }

                    DrawCommand(LPCVOID instanceData, UINT32 instanceStride, UINT32 instanceCount, Video3D::BufferInterface *vertexBuffer, UINT32 firstVertex, Video3D::BufferInterface *indexBuffer, UINT32 indexCount, UINT32 firstIndex)
                        : instanceData(instanceData)
                        , instanceStride(instanceStride)
                        , instanceCount(instanceCount)
                        , vertexBuffer(vertexBuffer)
                        , firstVertex(firstVertex)
                        , indexBuffer(indexBuffer)
                        , indexCount(indexCount)
                        , firstIndex(firstIndex)
                        , vertexCount(0)
                    {
                    }
                };

            private:
                IUnknown *initializerContext;
                Video3D::Interface *video;
                Population::Interface *population;

                CComPtr<Video3D::BufferInterface> cameraConstantBuffer;
                CComPtr<Video3D::BufferInterface> lightingConstantBuffer;
                CComPtr<Video3D::BufferInterface> lightingBuffer;
                CComPtr<Video3D::BufferInterface> instanceBuffer;

                concurrency::concurrent_unordered_map<IUnknown *, concurrency::concurrent_unordered_map<IUnknown *, concurrency::concurrent_vector<DrawCommand>>> drawQueue;
                concurrency::concurrent_unordered_map<std::size_t, CComPtr<IUnknown>> resourceMap;

            public:
                System(void)
                    : initializerContext(nullptr)
                    , video(nullptr)
                    , population(nullptr)
                {
                }

                ~System(void)
                {
                    BaseObservable::removeObserver(population, getClass<Engine::Population::Observer>());
                }

                BEGIN_INTERFACE_LIST(System)
                    INTERFACE_LIST_ENTRY_COM(Gek::ObservableInterface)
                    INTERFACE_LIST_ENTRY_COM(Population::Observer)
                    INTERFACE_LIST_ENTRY_COM(Interface)
                END_INTERFACE_LIST_USER

                HRESULT getResource(IUnknown **returnObject, std::size_t hash, std::function<HRESULT(IUnknown **)> onResourceMissing)
                {
                    HRESULT returnValue = E_FAIL;
                    auto resourceIterator = resourceMap.find(hash);
                    if (resourceIterator != resourceMap.end())
                    {
                        returnValue = (*resourceIterator).second->QueryInterface(IID_PPV_ARGS(returnObject));
                    }
                    else
                    {
                        CComPtr<IUnknown> &resource = resourceMap[hash];
                        returnValue = onResourceMissing(&resource);
                        if (SUCCEEDED(returnValue) && resource)
                        {
                            returnValue = resource->QueryInterface(IID_PPV_ARGS(returnObject));
                        }
                    }

                    return returnValue;
                }

                // Render::Interface
                STDMETHODIMP initialize(IUnknown *initializerContext)
                {
                    REQUIRE_RETURN(initializerContext, E_INVALIDARG);

                    HRESULT resultValue = E_FAIL;
                    CComQIPtr<Video3D::Interface> video(initializerContext);
                    CComQIPtr<Population::Interface> population(initializerContext);
                    if (video && population)
                    {
                        this->video = video;
                        this->population = population;
                        this->initializerContext = initializerContext;
                        resultValue = BaseObservable::addObserver(population, getClass<Engine::Population::Observer>());
                    }

                    if (SUCCEEDED(resultValue))
                    {
                        resultValue = video->createBuffer(&cameraConstantBuffer, sizeof(CameraConstantBuffer), 1, Video3D::BufferFlags::CONSTANT_BUFFER);
                    }

                    if (SUCCEEDED(resultValue))
                    {
                        resultValue = video->createBuffer(&lightingConstantBuffer, sizeof(LightingConstantBuffer), 1, Video3D::BufferFlags::CONSTANT_BUFFER);
                    }

                    if (SUCCEEDED(resultValue))
                    {
                        resultValue = video->createBuffer(&lightingBuffer, sizeof(Light), 256, Video3D::BufferFlags::DYNAMIC | Video3D::BufferFlags::STRUCTURED_BUFFER | Video3D::BufferFlags::RESOURCE);
                    }

                    if (SUCCEEDED(resultValue))
                    {
                        resultValue = video->createBuffer(&instanceBuffer, sizeof(float), 1024, Video3D::BufferFlags::DYNAMIC | Video3D::BufferFlags::STRUCTURED_BUFFER | Video3D::BufferFlags::RESOURCE);
                    }

                    return resultValue;
                }

                STDMETHODIMP loadPlugin(IUnknown **returnObject, LPCWSTR fileName)
                {
                    REQUIRE_RETURN(initializerContext, E_FAIL);

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, std::hash<LPCWSTR>()(fileName), [&](IUnknown **returnObject) -> HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        CComPtr<Plugin::Interface> plugin;
                        returnValue = getContext()->createInstance(CLSID_IID_PPV_ARGS(Plugin::Class, &plugin));
                        if (plugin)
                        {
                            returnValue = plugin->initialize(initializerContext, fileName);
                            if (SUCCEEDED(returnValue))
                            {
                                returnValue = plugin->QueryInterface(returnObject);
                            }
                        }

                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP loadShader(IUnknown **returnObject, LPCWSTR fileName)
                {
                    REQUIRE_RETURN(initializerContext, E_FAIL);

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, std::hash<LPCWSTR>()(fileName), [&](IUnknown **returnObject) -> HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        CComPtr<Shader::Interface> shader;
                        returnValue = getContext()->createInstance(CLSID_IID_PPV_ARGS(Shader::Class, &shader));
                        if (shader)
                        {
                            returnValue = shader->initialize(initializerContext, fileName);
                            if (SUCCEEDED(returnValue))
                            {
                                returnValue = shader->QueryInterface(returnObject);
                            }
                        }

                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP loadMaterial(IUnknown **returnObject, LPCWSTR fileName)
                {
                    REQUIRE_RETURN(initializerContext, E_FAIL);

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, std::hash<LPCWSTR>()(fileName), [&](IUnknown **returnObject) -> HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        CComPtr<Material::Interface> material;
                        returnValue = getContext()->createInstance(CLSID_IID_PPV_ARGS(Material::Class, &material));
                        if (material)
                        {
                            returnValue = material->initialize(initializerContext, fileName);
                            if (SUCCEEDED(returnValue))
                            {
                                returnValue = material->QueryInterface(returnObject);
                            }
                        }

                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP createRenderStates(IUnknown **returnObject, const Video3D::RenderStates &renderStates)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    std::size_t hash = std::hashCombine(static_cast<UINT8>(renderStates.fillMode),
                        static_cast<UINT8>(renderStates.cullMode),
                        renderStates.frontCounterClockwise,
                        renderStates.depthBias,
                        renderStates.depthBiasClamp,
                        renderStates.slopeScaledDepthBias,
                        renderStates.depthClipEnable,
                        renderStates.scissorEnable,
                        renderStates.multisampleEnable,
                        renderStates.antialiasedLineEnable);

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, hash, [&](IUnknown **returnObject)->HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        returnValue = video->createRenderStates(returnObject, renderStates);
                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP createDepthStates(IUnknown **returnObject, const Video3D::DepthStates &depthStates)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    std::size_t hash = std::hashCombine(depthStates.enable,
                        static_cast<UINT8>(depthStates.writeMask),
                        static_cast<UINT8>(depthStates.comparisonFunction),
                        depthStates.stencilEnable,
                        depthStates.stencilReadMask,
                        depthStates.stencilWriteMask,
                        static_cast<UINT8>(depthStates.stencilFrontStates.failOperation),
                        static_cast<UINT8>(depthStates.stencilFrontStates.depthFailOperation),
                        static_cast<UINT8>(depthStates.stencilFrontStates.passOperation),
                        static_cast<UINT8>(depthStates.stencilFrontStates.comparisonFunction),
                        static_cast<UINT8>(depthStates.stencilBackStates.failOperation),
                        static_cast<UINT8>(depthStates.stencilBackStates.depthFailOperation),
                        static_cast<UINT8>(depthStates.stencilBackStates.passOperation),
                        static_cast<UINT8>(depthStates.stencilBackStates.comparisonFunction));

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, hash, [&](IUnknown **returnObject)->HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        returnValue = video->createDepthStates(returnObject, depthStates);
                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP createBlendStates(IUnknown **returnObject, const Video3D::UnifiedBlendStates &blendStates)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    std::size_t hash = std::hashCombine(blendStates.enable,
                        static_cast<UINT8>(blendStates.colorSource),
                        static_cast<UINT8>(blendStates.colorDestination),
                        static_cast<UINT8>(blendStates.colorOperation),
                        static_cast<UINT8>(blendStates.alphaSource),
                        static_cast<UINT8>(blendStates.alphaDestination),
                        static_cast<UINT8>(blendStates.alphaOperation),
                        blendStates.writeMask);

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, hash, [&](IUnknown **returnObject)->HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        returnValue = video->createBlendStates(returnObject, blendStates);
                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP createBlendStates(IUnknown **returnObject, const Video3D::IndependentBlendStates &blendStates)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    std::size_t hash = 0;
                    for (UINT32 renderTarget = 0; renderTarget < 8; ++renderTarget)
                    {
                        std::hashCombine(hash, std::hashCombine(blendStates.targetStates[renderTarget].enable,
                            static_cast<UINT8>(blendStates.targetStates[renderTarget].colorSource),
                            static_cast<UINT8>(blendStates.targetStates[renderTarget].colorDestination),
                            static_cast<UINT8>(blendStates.targetStates[renderTarget].colorOperation),
                            static_cast<UINT8>(blendStates.targetStates[renderTarget].alphaSource),
                            static_cast<UINT8>(blendStates.targetStates[renderTarget].alphaDestination),
                            static_cast<UINT8>(blendStates.targetStates[renderTarget].alphaOperation),
                            blendStates.targetStates[renderTarget].writeMask));
                    }

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, hash, [&](IUnknown **returnObject)->HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        returnValue = video->createBlendStates(returnObject, blendStates);
                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP createRenderTarget(Video3D::TextureInterface **returnObject, UINT32 width, UINT32 height, Video3D::Format format)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    HRESULT returnValue = E_FAIL;
                    returnValue = video->createRenderTarget(returnObject, width, height, format);
                    return returnValue;
                }

                STDMETHODIMP createDepthTarget(IUnknown **returnObject, UINT32 width, UINT32 height, Video3D::Format format)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    HRESULT returnValue = E_FAIL;
                    returnValue = video->createDepthTarget(returnObject, width, height, format);
                    return returnValue;
                }

                STDMETHODIMP createBuffer(Video3D::BufferInterface **returnObject, Video3D::Format format, UINT32 count, UINT32 flags, LPCVOID staticData)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    HRESULT returnValue = E_FAIL;
                    returnValue = video->createBuffer(returnObject, format, count, flags, staticData);
                    return returnValue;
                }

                STDMETHODIMP loadComputeProgram(IUnknown **returnObject, LPCWSTR fileName, LPCSTR entryFunction, std::function<HRESULT(LPCSTR, std::vector<UINT8> &)> onInclude, std::unordered_map<CStringA, CStringA> *defineList)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    std::size_t hash = std::hashCombine(fileName, entryFunction);
                    if (defineList)
                    {
                        for (auto &define : (*defineList))
                        {
                            hash = std::hashCombine(hash, std::hashCombine(define.first, define.second));
                        }
                    }

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, hash, [&](IUnknown **returnObject)->HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        returnValue = video->loadComputeProgram(returnObject, fileName, entryFunction, onInclude, defineList);
                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP loadPixelProgram(IUnknown **returnObject, LPCWSTR fileName, LPCSTR entryFunction, std::function<HRESULT(LPCSTR, std::vector<UINT8> &)> onInclude, std::unordered_map<CStringA, CStringA> *defineList)
                {
                    REQUIRE_RETURN(video, E_FAIL);

                    std::size_t hash = std::hashCombine(fileName, entryFunction);
                    if (defineList)
                    {
                        for (auto &define : (*defineList))
                        {
                            hash = std::hashCombine(hash, std::hashCombine(define.first, define.second));
                        }
                    }

                    HRESULT returnValue = E_FAIL;
                    returnValue = getResource(returnObject, hash, [&](IUnknown **returnObject)->HRESULT
                    {
                        HRESULT returnValue = E_FAIL;
                        returnValue = video->loadPixelProgram(returnObject, fileName, entryFunction, onInclude, defineList);
                        return returnValue;
                    });

                    return returnValue;
                }

                STDMETHODIMP_(void) drawPrimitive(IUnknown *pluginHandle, IUnknown *materialHandle, Video3D::BufferInterface *vertexBuffer, UINT32 vertexCount, UINT32 firstVertex)
                {
                    drawQueue[pluginHandle][materialHandle].push_back(DrawCommand(vertexBuffer, vertexCount, firstVertex));
                }

                STDMETHODIMP_(void) drawIndexedPrimitive(IUnknown *pluginHandle, IUnknown *materialHandle, Video3D::BufferInterface *vertexBuffer, UINT32 firstVertex, Video3D::BufferInterface *indexBuffer, UINT32 indexCount, UINT32 firstIndex)
                {
                    drawQueue[pluginHandle][materialHandle].push_back(DrawCommand(vertexBuffer, firstVertex, indexBuffer, indexCount, firstIndex));
                }

                STDMETHODIMP_(void) drawInstancedPrimitive(IUnknown *pluginHandle, IUnknown *materialHandle, LPCVOID instanceData, UINT32 instanceStride, UINT32 instanceCount, Video3D::BufferInterface *vertexBuffer, UINT32 vertexCount, UINT32 firstVertex)
                {
                    drawQueue[pluginHandle][materialHandle].push_back(DrawCommand(instanceData, instanceStride, instanceCount, vertexBuffer, vertexCount, firstVertex));
                }

                STDMETHODIMP_(void) drawInstancedIndexedPrimitive(IUnknown *pluginHandle, IUnknown *materialHandle, LPCVOID instanceData, UINT32 instanceStride, UINT32 instanceCount, Video3D::BufferInterface *vertexBuffer, UINT32 firstVertex, Video3D::BufferInterface *indexBuffer, UINT32 indexCount, UINT32 firstIndex)
                {
                    drawQueue[pluginHandle][materialHandle].push_back(DrawCommand(instanceData, instanceStride, instanceCount, vertexBuffer, firstVertex, indexBuffer, indexCount, firstIndex));
                }

                // Population::Observer
                STDMETHODIMP_(void) onLoadBegin(void)
                {
                }

                STDMETHODIMP_(void) onLoadEnd(HRESULT resultValue)
                {
                    if (FAILED(resultValue))
                    {
                        onFree();
                    }
                }

                STDMETHODIMP_(void) onFree(void)
                {
                    REQUIRE_VOID_RETURN(video);
                }

                STDMETHODIMP_(void) onUpdateEnd(float frameTime)
                {
                    if (frameTime > 0.0f)
                    {
                        video->clearDefaultRenderTarget(Math::Float4(0.0f, 1.0f, 0.0f, 1.0f));
                    }
                    else
                    {
                        video->clearDefaultRenderTarget(Math::Float4(1.0f, 0.0f, 0.0f, 1.0f));
                    }

                    population->listEntities({ Components::Transform::identifier, Components::Camera::identifier }, [&](Population::Entity cameraEntity) -> void
                    {
                        auto &transformComponent = population->getComponent<Components::Transform::Data>(cameraEntity, Components::Transform::identifier);
                        auto &cameraComponent = population->getComponent<Components::Camera::Data>(cameraEntity, Components::Camera::identifier);
                        Math::Float4x4 cameraMatrix(transformComponent.rotation, transformComponent.position);

                        CameraConstantBuffer cameraConstantBuffer;
                        float displayAspectRatio = 1.0f;
                        float fieldOfView = Math::convertDegreesToRadians(cameraComponent.fieldOfView);
                        cameraConstantBuffer.fieldOfView.x = tan(fieldOfView * 0.5f);
                        cameraConstantBuffer.fieldOfView.y = (cameraConstantBuffer.fieldOfView.x / displayAspectRatio);
                        cameraConstantBuffer.minimumDistance = cameraComponent.minimumDistance;
                        cameraConstantBuffer.maximumDistance = cameraComponent.maximumDistance;
                        cameraConstantBuffer.viewMatrix = cameraMatrix.getInverse();
                        cameraConstantBuffer.projectionMatrix.setPerspective(fieldOfView, displayAspectRatio, cameraComponent.minimumDistance, cameraComponent.maximumDistance);
                        cameraConstantBuffer.inverseProjectionMatrix = cameraConstantBuffer.projectionMatrix.getInverse();
                        cameraConstantBuffer.transformMatrix = (cameraConstantBuffer.viewMatrix * cameraConstantBuffer.projectionMatrix);
                        video->updateBuffer(this->cameraConstantBuffer, &cameraConstantBuffer);

                        Shape::Frustum viewFrustum(cameraConstantBuffer.transformMatrix, transformComponent.position);

                        concurrency::concurrent_vector<Light> concurrentVisibleLightList;
                        population->listEntities({ Components::Transform::identifier, Components::PointLight::identifier, Components::Color::identifier }, [&](Population::Entity lightEntity) -> void
                        {
                            auto &transformComponent = population->getComponent<Components::Transform::Data>(lightEntity, Components::Transform::identifier);
                            auto &pointLightComponent = population->getComponent<Components::PointLight::Data>(lightEntity, Components::PointLight::identifier);
                            if (viewFrustum.isVisible(Shape::Sphere(transformComponent.position, pointLightComponent.radius)))
                            {
                                auto lightIterator = concurrentVisibleLightList.grow_by(1);
                                (*lightIterator).position = (cameraConstantBuffer.viewMatrix * Math::Float4(transformComponent.position, 1.0f));
                                (*lightIterator).distance = (*lightIterator).position.getLengthSquared();
                                (*lightIterator).range = pointLightComponent.radius;
                                (*lightIterator).color = population->getComponent<Components::Color::Data>(lightEntity, Components::Color::identifier);
                            }
                        }, true);

                        concurrency::parallel_sort(concurrentVisibleLightList.begin(), concurrentVisibleLightList.end(), [](const Light &leftLight, const Light &rightLight) -> bool
                        {
                            return (leftLight.distance < rightLight.distance);
                        });

                        std::vector<Light> visibleLightList(concurrentVisibleLightList.begin(), concurrentVisibleLightList.end());
                        concurrentVisibleLightList.clear();

                        drawQueue.clear();
                        BaseObservable::sendEvent(Event<Render::Observer>(std::bind(&Render::Observer::OnRenderScene, std::placeholders::_1, cameraEntity, viewFrustum)));
                        for (auto &pluginPair : drawQueue)
                        {
                            IUnknown *pluginHandle = pluginPair.first;
                            for (auto &materialPair : pluginPair.second)
                            {
                                IUnknown * materialHandle = materialPair.first;
                                for (auto &drawCommand : materialPair.second)
                                {
                                }
                            }
                        }
                    });

                    BaseObservable::sendEvent(Event<Render::Observer>(std::bind(&Render::Observer::onRenderOverlay, std::placeholders::_1)));

                    video->present(true);
                }
            };

            REGISTER_CLASS(System)
        }; // namespace Render
    }; // namespace Engine
}; // namespace Gek
