﻿#include "GEK\Math\Float4x4.h"
#include "GEK\Shapes\AlignedBox.h"
#include "GEK\Shapes\OrientedBox.h"
#include "GEK\Utility\FileSystem.h"
#include "GEK\Utility\String.h"
#include "GEK\Utility\XML.h"
#include "GEK\Utility\Allocator.h"
#include "GEK\Context\ContextUser.h"
#include "GEK\Context\ObservableMixin.h"
#include "GEK\System\VideoDevice.h"
#include "GEK\Engine\Core.h"
#include "GEK\Engine\ComponentMixin.h"
#include "GEK\Engine\Processor.h"
#include "GEK\Engine\Population.h"
#include "GEK\Engine\Entity.h"
#include "GEK\Engine\Renderer.h"
#include "GEK\Engine\Resources.h"
#include "GEK\Components\Transform.h"
#include "GEK\Components\Color.h"
#include <concurrent_queue.h>
#include <concurrent_unordered_map.h>
#include <concurrent_vector.h>
#include <ppl.h>
#include <algorithm>
#include <memory>
#include <future>
#include <array>
#include <map>

namespace Gek
{
#ifdef _ENCODE_SPHEREMAP
    Math::Float2 encodeNormal(Math::Float3 normal)
    {
        Math::Float2 encoded(normal.x, normal.y);
        return encoded.getNormal() * std::sqrt(normal.z * 0.5f + 0.5f);
    }
#elif _ENCODE_OCTAHEDRON
    Math::Float2 octahedronWrap(Math::Float2 value)
    {
        return (1.0f - std::abs(value)) * (value >= 0.0f ? 1.0f : -1.0f);
    }

    Math::Float2 encodeNormal((Math::Float3 normal)
    {
        normal /= (std::abs(normal.x) + std::abs(normal.y) + std::abs(normal.z));

        Math::Float2 encoded(normal.x, normal.y);
        encoded = normal.z >= 0.0f ? encoded : OctWrap(encoded);
        encoded = encoded * 0.5f + 0.5f;
        return encoded;
    }
#else
    Math::Float2 encodeNormal(Math::Float3 normal)
    {
        return Math::Float2(normal.y, normal.z);
    }
#endif

    struct Vertex
    {
        Math::Float3 position;
        Math::Float2 texCoord;
        Math::Float3 normal;

        Vertex(void)
        {
        }

        Vertex(const Math::Float3 &position)
            : position(position)
            , normal(position)
            , texCoord(encodeNormal(position))
        {
        }
    };

    class GeoSphere
    {
    private:
        struct Edge
        {
            bool split; // whether this edge has been split
            std::array <size_t, 2> vertices; // the two endpoint vertex indices
            size_t splitVertex; // the split vertex index
            std::array <size_t, 2> splitEdges; // the two edges after splitting - they correspond to the "vertices" vector

            Edge(size_t v0, size_t v1)
                : vertices{ v0, v1 }
            {
                split = false;
            }

            size_t getSplitEdge(size_t vertex)
            {
                if (vertex == vertices[0])
                {
                    return splitEdges[0];
                }
                else if (vertex == vertices[1])
                {
                    return splitEdges[1];
                }
                else
                {
                    return 0;
                }
            }
        };

        struct Triangle
        {
            std::array <size_t, 3> vertices; // the vertex indices
            std::array <size_t, 3> edges; // the edge indices

            Triangle(size_t v0, size_t v1, size_t v2, size_t e0, size_t e1, size_t e2)
                : vertices{ v0, v1, v2 }
                , edges{ e0, e1, e2 }
            {
            }
        };

        std::vector <Vertex> vertices;
        std::vector <Edge> edges;
        std::vector <Edge> newEdges;
        std::vector <Triangle> triangles;
        std::vector <Triangle> newTriangles;

        std::vector <uint16_t> indices;
        float inscriptionRadiusMultiplier;

        void splitEdge(Edge & e)
        {
            if (e.split)
            {
                return;
            }

            e.splitVertex = vertices.size();
            vertices.push_back(Vertex((Math::Float3(0.5f) * (vertices[e.vertices[0]].position + vertices[e.vertices[1]].position)).getNormal()));
            e.splitEdges = { newEdges.size(), newEdges.size() + 1 };
            newEdges.push_back(Edge(e.vertices[0], e.splitVertex));
            newEdges.push_back(Edge(e.splitVertex, e.vertices[1]));
            e.split = true;
        }

        void subdivideTriangle(const Triangle & t)
        {
            //    0
            //    /\
            //  0/  \2
            //  /____\
            // 1  1   2
            size_t edge0 = t.edges[0];
            size_t edge1 = t.edges[1];
            size_t edge2 = t.edges[2];
            size_t vert0 = t.vertices[0];
            size_t vert1 = t.vertices[1];
            size_t vert2 = t.vertices[2];
            splitEdge(edges[edge0]);
            splitEdge(edges[edge1]);
            splitEdge(edges[edge2]);
            size_t edge01 = newEdges.size();
            size_t edge12 = newEdges.size() + 1;
            size_t edge20 = newEdges.size() + 2;
            newEdges.push_back(Edge(edges[edge0].splitVertex, edges[edge1].splitVertex));
            newEdges.push_back(Edge(edges[edge1].splitVertex, edges[edge2].splitVertex));
            newEdges.push_back(Edge(edges[edge2].splitVertex, edges[edge0].splitVertex));

            // important: we push the "center" triangle first
            // this is so a center-most triangle is guaranteed to be at index 0 of the final object
            newTriangles.push_back(Triangle(edges[edge0].splitVertex, edges[edge1].splitVertex, edges[edge2].splitVertex, edge01, edge12, edge20));

            newTriangles.push_back(Triangle(vert0, edges[edge0].splitVertex, edges[edge2].splitVertex, edges[edge0].getSplitEdge(vert0), edge20, edges[edge2].getSplitEdge(vert0)));

            newTriangles.push_back(Triangle(edges[edge0].splitVertex, vert1, edges[edge1].splitVertex, edges[edge0].getSplitEdge(vert1), edges[edge1].getSplitEdge(vert1), edge01));

            newTriangles.push_back(Triangle(edges[edge2].splitVertex, edges[edge1].splitVertex, vert2, edge12, edges[edge1].getSplitEdge(vert2), edges[edge2].getSplitEdge(vert2)));
        }

        void subdivide()
        {
            size_t trianglesCount = triangles.size();
            for (size_t i = 0; i < trianglesCount; ++i)
            {
                subdivideTriangle(triangles[i]);
            }

            edges.swap(newEdges);
            triangles.swap(newTriangles);
            newEdges.clear();
            newTriangles.clear();
        }

        float computeInscriptionRadiusMultiplier() const
        {
            // all triangles have 3 points with norm 1
            // this means for each triangle, the point on the plane
            // with the smallest norm is the centroid (average) of
            // the three vertices. Thus, since all vertices have
            // norm 1, the largest triangle will have a centroid
            // with mean closest to 0. Each time we subdivide a triangle,
            // we get 3 triangles with 2 points "pushed out" (the surrounding
            // triangles) and one triangle with all 3 points "pushed out"
            // (the center triangle). Since pushing out a point makes the
            // triangle bigger, the biggest triangle must be the
            // center-most triangle - i.e. take the path of the center
            // triangle in each subdivision to get the biggest triangle.
            // In the subdivideTriangle() method, we ensured the center
            // triangle is always pushed to the new triangle array first -
            // thus, one of the center triangles must have index 0. Therefore
            // we compute the centroid of the triangle with index 0
            // and take its norm. If we divide by this value, "expand" the
            // sphere to contain a given radius rather than be contained by
            // a given radius. However, since division is slower, we'd rather
            // multiply, so we return the reciprocal.
            // 1/3 times the sum of the three vertices
            static const Math::Float3 oneThird(1.0f / 3.0f);
            Math::Float3 centroid(oneThird * (vertices[triangles[0].vertices[0]].position + vertices[triangles[0].vertices[1]].position + vertices[triangles[0].vertices[2]].position));
            return (1.0f / centroid.getLength());
        }

    public:
        GeoSphere()
        {
        }

        void generate(size_t subdivisions)
        {
            static const float PHI = 1.618033988749894848204586834365638f;

            static const Vertex initialVertices[] =
            {
                Vertex(Math::Float3(-1.0f,  0.0f,   PHI).getNormal()),  // 0
                Vertex(Math::Float3(1.0f,  0.0f,   PHI).getNormal()),  // 1
                Vertex(Math::Float3(0.0f,   PHI,  1.0f).getNormal()),  // 2
                Vertex(Math::Float3(-PHI,  1.0f,  0.0f).getNormal()),  // 3
                Vertex(Math::Float3(-PHI, -1.0f,  0.0f).getNormal()),  // 4
                Vertex(Math::Float3(0.0f,  -PHI,  1.0f).getNormal()),  // 5
                Vertex(Math::Float3(PHI, -1.0f,  0.0f).getNormal()),  // 6
                Vertex(Math::Float3(PHI,  1.0f,  0.0f).getNormal()),  // 7
                Vertex(Math::Float3(0.0f,   PHI, -1.0f).getNormal()),  // 8
                Vertex(Math::Float3(-1.0f,  0.0f,  -PHI).getNormal()),  // 9
                Vertex(Math::Float3(0.0f,  -PHI, -1.0f).getNormal()),  // 10
                Vertex(Math::Float3(1.0f,  0.0f,  -PHI).getNormal()),  // 11
            };

            static const Edge initialEdges[] =
            {
                Edge(0,  1),   // 0
                Edge(0,  2),   // 1
                Edge(0,  3),   // 2
                Edge(0,  4),   // 3
                Edge(0,  5),   // 4
                Edge(1,  2),   // 5
                Edge(2,  3),   // 6
                Edge(3,  4),   // 7
                Edge(4,  5),   // 8
                Edge(5,  1),   // 9
                Edge(5,  6),   // 10
                Edge(6,  1),   // 11
                Edge(1,  7),   // 12
                Edge(7,  2),   // 13
                Edge(2,  8),   // 14
                Edge(8,  3),   // 15
                Edge(3,  9),   // 16
                Edge(9,  4),   // 17
                Edge(4, 10),   // 18
                Edge(10,  5),   // 19
                Edge(10,  6),   // 20
                Edge(6,  7),   // 21
                Edge(7,  8),   // 22
                Edge(8,  9),   // 23
                Edge(9, 10),   // 24
                Edge(10, 11),   // 25
                Edge(6, 11),   // 26
                Edge(7, 11),   // 27
                Edge(8, 11),   // 28
                Edge(9, 11),   // 29
            };

            static const Triangle initialTriangles[] =
            {
                Triangle(0,  1,  2,  0,  5,  1),
                Triangle(0,  2,  3,  1,  6,  2),
                Triangle(0,  3,  4,  2,  7,  3),
                Triangle(0,  4,  5,  3,  8,  4),
                Triangle(0,  5,  1,  4,  9,  0),
                Triangle(1,  6,  7, 11, 21, 12),
                Triangle(1,  7,  2, 12, 13,  5),
                Triangle(2,  7,  8, 13, 22, 14),
                Triangle(2,  8,  3, 14, 15,  6),
                Triangle(3,  8,  9, 15, 23, 16),
                Triangle(3,  9,  4, 16, 17,  7),
                Triangle(4,  9, 10, 17, 24, 18),
                Triangle(4, 10,  5, 18, 19,  8),
                Triangle(5, 10,  6, 19, 20, 10),
                Triangle(5,  6,  1, 10, 11,  9),
                Triangle(6, 11,  7, 26, 27, 21),
                Triangle(7, 11,  8, 27, 28, 22),
                Triangle(8, 11,  9, 28, 29, 23),
                Triangle(9, 11, 10, 29, 25, 24),
                Triangle(10, 11,  6, 25, 26, 20),
            };

            vertices.clear();
            edges.clear();
            triangles.clear();

            size_t vertexCount = ARRAYSIZE(initialVertices);
            size_t edgeCount = ARRAYSIZE(initialEdges);
            size_t triangleCount = ARRAYSIZE(initialTriangles);

            // reserve space
            for (size_t i = 0; i < subdivisions; ++i)
            {
                vertexCount += edgeCount;
                edgeCount = edgeCount * 2 + triangleCount * 3;
                triangleCount *= 4;
            }

            vertices.reserve(vertexCount);
            edges.reserve(edgeCount);
            newEdges.reserve(edgeCount);
            triangles.reserve(triangleCount);
            newTriangles.reserve(triangleCount);

            vertices.assign(initialVertices, initialVertices + ARRAYSIZE(initialVertices));
            edges.assign(initialEdges, initialEdges + ARRAYSIZE(initialEdges));
            triangles.assign(initialTriangles, initialTriangles + ARRAYSIZE(initialTriangles));

            for (size_t i = 0; i < subdivisions; ++i)
            {
                subdivide();
            }

            inscriptionRadiusMultiplier = computeInscriptionRadiusMultiplier();

            // now we create the array of indices
            size_t trianglesCount = triangles.size();
            indices.reserve(trianglesCount * 3);
            for (size_t i = 0; i < trianglesCount; ++i)
            {
                indices.push_back((uint16_t)triangles[i].vertices[0]);
                indices.push_back((uint16_t)triangles[i].vertices[1]);
                indices.push_back((uint16_t)triangles[i].vertices[2]);
            }
        }

        const std::vector <Vertex> & getVertices() const
        {
            return vertices;
        }

        const std::vector <uint16_t> & getIndices() const
        {
            return indices;
        }

        float getInscriptionRadiusMultiplier() const
        {
            return inscriptionRadiusMultiplier;
        }
    };

    namespace Components
    {
        struct Shape
        {
            String value;
            String parameters;
            String skin;

            Shape(void)
            {
            }

            void save(Plugin::Population::ComponentDefinition &componentData) const
            {
                saveParameter(componentData, nullptr, value);
                saveParameter(componentData, L"parameters", parameters);
                saveParameter(componentData, L"skin", skin);
            }

            void load(const Plugin::Population::ComponentDefinition &componentData)
            {
                value = loadParameter(componentData, nullptr, String());
                parameters = loadParameter(componentData, L"parameters", String());
                skin = loadParameter(componentData, L"skin", String());
            }
        };
    }; // namespace Components

    GEK_CONTEXT_USER(Shape)
        , public Plugin::ComponentMixin<Components::Shape>
    {
    public:
        Shape(Context *context)
            : ContextRegistration(context)
        {
        }

        // Plugin::Component
        const wchar_t * const getName(void) const
        {
            return L"shape";
        }
    };

    GEK_CONTEXT_USER(ShapeProcessor, Plugin::Core *)
        , public Plugin::PopulationObserver
        , public Plugin::RendererObserver
        , public Plugin::Processor
    {
    public:
        enum class ShapeType : uint8_t
        {
            Unknown = 0,
            Sphere,
        };

        struct Shape
        {
            ShapeType type;
            String parameters;
            std::atomic<bool> loaded;
            Shapes::AlignedBox alignedBox;
            ResourceHandle vertexBuffer;
            ResourceHandle indexBuffer;
            uint32_t indexCount;

            Shape(void)
                : type(ShapeType::Unknown)
                , loaded(false)
                , indexCount(0)
            {
            }

            Shape(const Shape &shape)
                : type(shape.type)
                , parameters(shape.parameters)
                , loaded(shape.loaded ? true : false)
                , alignedBox(shape.alignedBox)
                , vertexBuffer(shape.vertexBuffer)
                , indexBuffer(shape.indexBuffer)
                , indexCount(shape.indexCount)
            {
            }
        };

        struct Data
        {
            Shape &shape;
            MaterialHandle skin;
            const Math::Color &color;

            Data(Shape &shape, MaterialHandle skin, const Math::Color &color)
                : shape(shape)
                , skin(skin)
                , color(color)
            {
            }
        };

        __declspec(align(16))
            struct Instance
        {
            Math::Float4x4 matrix;
            Math::Color color;
            Math::Float3 scale;
            float buffer;

            Instance(const Math::Float4x4 &matrix, const Math::Color &color, const Math::Float3 &scale)
                : matrix(matrix)
                , color(color)
                , scale(scale)
            {
            }
        };

    private:
        Plugin::Population *population;
        Plugin::Resources *resources;
        Plugin::Renderer *renderer;

        VisualHandle visual;
        ResourceHandle constantBuffer;

        std::future<void> loadShapeRunning;
        concurrency::concurrent_queue<std::function<void(void)>> loadShapeQueue;
        concurrency::concurrent_unordered_map<std::size_t, bool> loadShapeMap;

        std::unordered_map<std::size_t, Shape> shapeMap;
        using EntityDataMap = std::unordered_map<Plugin::Entity *, Data>;
        EntityDataMap entityDataMap;

        using InstanceList = concurrency::concurrent_vector<Instance, AlignedAllocator<Instance, 16>>;
        using MaterialMap = concurrency::concurrent_unordered_map<MaterialHandle, InstanceList>;
        using VisibleMap = concurrency::concurrent_unordered_map<Shape *, MaterialMap>;
        VisibleMap visibleMap;

    public:
        ShapeProcessor(Context *context, Plugin::Core *core)
            : ContextRegistration(context)
            , population(core->getPopulation())
            , resources(core->getResources())
            , renderer(core->getRenderer())
        {
            population->addObserver((Plugin::PopulationObserver *)this);
            renderer->addObserver((Plugin::RendererObserver *)this);

            visual = resources->loadPlugin(L"model");

            constantBuffer = resources->createBuffer(nullptr, sizeof(Instance), 1, Video::BufferType::Constant, Video::BufferFlags::Mappable, false);
        }

        ~ShapeProcessor(void)
        {
            renderer->removeObserver((Plugin::RendererObserver *)this);
            population->removeObserver((Plugin::PopulationObserver *)this);
        }

        void loadBoundingBox(Shape &shape, const String &shapeName, const String &parameters)
        {
            if (shapeName.compareNoCase(L"sphere") == 0)
            {
                shape.type = ShapeType::Sphere;
                shape.parameters = parameters;
            }

            shape.alignedBox.minimum = Math::Float3(-1.0f);
            shape.alignedBox.maximum = Math::Float3(1.0f);
        }

        void loadShapeWorker(Shape &shape)
        {
            int position = 0;
            switch (shape.type)
            {
            case ShapeType::Sphere:
                if (true)
                {
                    GeoSphere geoSphere;
                    uint32_t divisionCount = shape.parameters;
                    geoSphere.generate(divisionCount);

                    shape.indexCount = geoSphere.getIndices().size();
                    shape.vertexBuffer = resources->createBuffer(String(L"shape:vertex:%v:%v", static_cast<uint8_t>(shape.type), shape.parameters), sizeof(Vertex), geoSphere.getVertices().size(), Video::BufferType::Vertex, 0, false, geoSphere.getVertices().data());
                    shape.indexBuffer = resources->createBuffer(String(L"shape:index:%v:%v", static_cast<uint8_t>(shape.type), shape.parameters), Video::Format::R16_UINT, geoSphere.getIndices().size(), Video::BufferType::Index, 0, false, geoSphere.getIndices().data());
                    break;
                }
            };

            shape.loaded = true;
        }

        void loadShape(Shape &shape)
        {
            std::size_t hash = reinterpret_cast<size_t>(&shape);
            if (loadShapeMap.count(hash) == 0)
            {
                loadShapeMap.insert(std::make_pair(hash, true));
                loadShapeQueue.push(std::bind(&ShapeProcessor::loadShapeWorker, this, std::ref(shape)));
                if (!loadShapeRunning.valid() || (loadShapeRunning.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready))
                {
                    loadShapeRunning = std::async(std::launch::async, [&](void) -> void
                    {
                        CoInitialize(nullptr);
                        std::function<void(void)> function;
                        while (loadShapeQueue.try_pop(function))
                        {
                            function();
                        };

                        CoUninitialize();
                    });
                }
            }
        }

        // Plugin::PopulationObserver
        void onLoadSucceeded(void)
        {
        }

        void onLoadFailed(void)
        {
            onFree();
        }

        void onFree(void)
        {
            loadShapeMap.clear();
            loadShapeQueue.clear();
            if (loadShapeRunning.valid() && (loadShapeRunning.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready))
            {
                loadShapeRunning.get();
            }

            shapeMap.clear();
            entityDataMap.clear();
        }

        void onEntityCreated(Plugin::Entity *entity)
        {
            GEK_REQUIRE(resources);
            GEK_REQUIRE(entity);

            if (entity->hasComponents<Components::Shape, Components::Transform>())
            {
                auto &shapeComponent = entity->getComponent<Components::Shape>();
                std::size_t hash = std::hash_combine(shapeComponent.parameters, shapeComponent.value);
                auto pair = shapeMap.insert(std::make_pair(hash, Shape()));
                if (pair.second)
                {
                    loadBoundingBox(pair.first->second, shapeComponent.value, shapeComponent.parameters);
                }

                std::reference_wrapper<const Math::Color> color = Math::Color::White;
                if (entity->hasComponent<Components::Color>())
                {
                    color = entity->getComponent<Components::Color>().value;
                }

                Data data(pair.first->second, resources->loadMaterial(shapeComponent.skin), color);
                entityDataMap.insert(std::make_pair(entity, data));
            }
        }

        void onEntityDestroyed(Plugin::Entity *entity)
        {
            GEK_REQUIRE(entity);

            auto entitySearch = entityDataMap.find(entity);
            if (entitySearch != entityDataMap.end())
            {
                entityDataMap.erase(entitySearch);
            }
        }

        // Plugin::RendererObserver
        static void drawCall(Video::Device::Context *deviceContext, Plugin::Resources *resources, const Shape *shape, const Instance *instanceList, ResourceHandle constantBuffer)
        {
            Instance *instanceData = nullptr;
            resources->mapBuffer(constantBuffer, (void **)&instanceData);
            memcpy(instanceData, instanceList, sizeof(Instance));
            resources->unmapBuffer(constantBuffer);

            resources->setConstantBuffer(deviceContext->vertexPipeline(), constantBuffer, 4);
            resources->setVertexBuffer(deviceContext, 0, shape->vertexBuffer, 0);
            resources->setIndexBuffer(deviceContext, shape->indexBuffer, 0);
            deviceContext->drawIndexedPrimitive(shape->indexCount, 0, 0);
        }

        void onRenderScene(Plugin::Entity *cameraEntity, const Math::Float4x4 *viewMatrix, const Shapes::Frustum *viewFrustum)
        {
            GEK_TRACE_SCOPE();
            GEK_REQUIRE(renderer);
            GEK_REQUIRE(cameraEntity);
            GEK_REQUIRE(viewFrustum);

            visibleMap.clear();
            concurrency::parallel_for_each(entityDataMap.begin(), entityDataMap.end(), [&](auto &entityDataPair) -> void
            {
                Plugin::Entity *entity = entityDataPair.first;
                Data &data = entityDataPair.second;
                Shape &shape = data.shape;

                const auto &transformComponent = entity->getComponent<Components::Transform>();
                Math::Float4x4 matrix(transformComponent.getMatrix());

                Shapes::OrientedBox orientedBox(shape.alignedBox, matrix);
                orientedBox.halfsize *= transformComponent.scale;

                if (viewFrustum->isVisible(orientedBox))
                {
                    auto &materialMap = visibleMap[&shape];
                    auto &instanceList = materialMap[data.skin];
                    instanceList.push_back(Instance((matrix * *viewMatrix), data.color, transformComponent.scale));
                }
            });

            concurrency::parallel_for_each(visibleMap.begin(), visibleMap.end(), [&](auto &visibleMap) -> void
            {
                Shape *shape = visibleMap.first;
                if (!shape->loaded)
                {
                    loadShape(*shape);
                    return;
                }

                concurrency::parallel_for_each(visibleMap.second.begin(), visibleMap.second.end(), [&](auto &materialMap) -> void
                {
                    concurrency::parallel_for_each(materialMap.second.begin(), materialMap.second.end(), [&](auto &instanceList) -> void
                    {
                        renderer->queueDrawCall(visual, materialMap.first, std::bind(drawCall, std::placeholders::_1, resources, shape, &instanceList, constantBuffer));
                    });
                });
            });
        }
    };

    GEK_REGISTER_CONTEXT_USER(Shape)
    GEK_REGISTER_CONTEXT_USER(ShapeProcessor)
}; // namespace Gek
