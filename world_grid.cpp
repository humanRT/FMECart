#include "world_grid.h"

#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <utils/EntityManager.h>

#include <math/vec3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "generated/resources/resources.h"

using namespace filament;
using namespace filament::math;

static inline void setLayer01Visible(Engine* engine, utils::Entity e) {
    auto& rcm = engine->getRenderableManager();
    auto inst = rcm.getInstance(e);
    if (inst) {
        rcm.setLayerMask(inst, 0xFF, 0x01);
    }
}

WorldGrid* WorldGrid::create(Engine* engine, Scene* scene, float halfSize, float spacing, float y, float thickness) {
    WorldGrid* grid = new WorldGrid();

    if (!grid->build(engine, scene, halfSize, spacing, y, thickness)) {
        delete grid;
        return nullptr;
    }

    return grid;
}

MaterialInstance* WorldGrid::createMaterial(Engine* engine, const float4& color) {
    Material* material = Material::Builder()
                                 .package(RESOURCES_SANDBOXUNLIT_DATA, RESOURCES_SANDBOXUNLIT_SIZE)
                                 .build(*engine);

    MaterialInstance* instance = material->createInstance();
    instance->setParameter("baseColor", color);

    mMaterials.push_back(material);
    mMaterialInstances.push_back(instance);

    return instance;
}

bool WorldGrid::build(Engine* engine, Scene* scene, float halfSize, float spacing, float y, float thickness) {
    halfSize = std::clamp(halfSize, 0.1f, 10.0f);
    spacing = std::max(0.01f, spacing);

    const int n = std::min(static_cast<int>(std::round(halfSize / spacing)), 800);
    const float extent = n * spacing;

    MaterialInstance* darkMaterial = createMaterial(engine, float4{ 0.13f, 0.23f, 0.38f, 1.0f });

    MaterialInstance* lightMaterial = createMaterial(engine, float4{ 0.31f, 0.48f, 0.67f, 1.0f });

    MaterialInstance* lineMaterial = createMaterial(engine, float4{ 0.92f, 0.96f, 1.00f, 1.0f });

    MaterialInstance* centerLineMaterial = createMaterial(engine, float4{ 1.0f, 1.0f, 1.0f, 1.0f });

    std::vector<float3> darkVerts;
    std::vector<float3> lightVerts;
    std::vector<uint32_t> darkIdx;
    std::vector<uint32_t> lightIdx;

    auto addTile = [](std::vector<float3>& vertices, std::vector<uint32_t>& indices, float x0,
                           float x1, float z0, float z1, float yBase) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back({ x0, yBase, z0 });
        vertices.push_back({ x0, yBase, z1 });
        vertices.push_back({ x1, yBase, z1 });
        vertices.push_back({ x1, yBase, z0 });

        indices.insert(indices.end(),
                { base + 0, base + 1, base + 2, base + 2, base + 3, base + 0 });
    };

    for (int i = -n; i < n; ++i) {
        for (int j = -n; j < n; ++j) {
            const float x0 = i * spacing;
            const float x1 = (i + 1) * spacing;
            const float z0 = j * spacing;
            const float z1 = (j + 1) * spacing;

            const bool light = ((i + j) & 1) == 0;

            if (light) {
                addTile(lightVerts, lightIdx, x0, x1, z0, z1, y);
            } else {
                addTile(darkVerts, darkIdx, x0, x1, z0, z1, y);
            }
        }
    }

    auto buildRenderable = [&](const std::vector<float3>& vertices,
                                   const std::vector<uint32_t>& indices,
                                   MaterialInstance* material) {
        if (vertices.empty() || indices.empty()) {
            return;
        }

        VertexBuffer* vertexBuffer = VertexBuffer::Builder()
                                             .vertexCount(static_cast<uint32_t>(vertices.size()))
                                             .bufferCount(1)
                                             .attribute(VertexAttribute::POSITION, 0,
                                                     VertexBuffer::AttributeType::FLOAT3)
                                             .build(*engine);

        auto* vertexCopy = new float3[vertices.size()];
        std::memcpy(vertexCopy, vertices.data(), vertices.size() * sizeof(float3));

        vertexBuffer->setBufferAt(*engine, 0,
                VertexBuffer::BufferDescriptor(vertexCopy, vertices.size() * sizeof(float3),
                        [](void* buffer, size_t, void*) {
                            delete[] static_cast<float3*>(buffer);
                        }));

        IndexBuffer* indexBuffer = IndexBuffer::Builder()
                                           .indexCount(static_cast<uint32_t>(indices.size()))
                                           .bufferType(IndexBuffer::IndexType::UINT)
                                           .build(*engine);

        auto* indexCopy = new uint32_t[indices.size()];
        std::memcpy(indexCopy, indices.data(), indices.size() * sizeof(uint32_t));

        indexBuffer->setBuffer(*engine,
                IndexBuffer::BufferDescriptor(indexCopy, indices.size() * sizeof(uint32_t),
                        [](void* buffer, size_t, void*) {
                            delete[] static_cast<uint32_t*>(buffer);
                        }));

        utils::Entity entity = utils::EntityManager::get().create();

        RenderableManager::Builder(1)
                .boundingBox({ { 0.0f, y, 0.0f }, { extent, 0.01f, extent } })
                .material(0, material)
                .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, vertexBuffer, indexBuffer)
                .culling(false)
                .receiveShadows(false)
                .castShadows(false)
                .build(*engine, entity);

        scene->addEntity(entity);
        setLayer01Visible(engine, entity);

        mEntities.push_back(entity);
        mVertexBuffers.push_back(vertexBuffer);
        mIndexBuffers.push_back(indexBuffer);
    };

    buildRenderable(darkVerts, darkIdx, darkMaterial);
    buildRenderable(lightVerts, lightIdx, lightMaterial);

    std::vector<float3> lineVerts;
    std::vector<uint32_t> lineIdx;

    std::vector<float3> centerLineVerts;
    std::vector<uint32_t> centerLineIdx;

    const float lineY = y + 0.0006f;
    const float centerLineY = y + 0.0012f;

    const float w = std::max(thickness, spacing * 0.01f);
    const float centerW = w * 4.0f;

    auto addLineQuadTo = [](std::vector<float3>& vertices, std::vector<uint32_t>& indices, float3 a,
                                 float3 b, float width) {
        float3 dir = b - a;
        const float len = length(dir);

        if (len < 1e-6f) {
            return;
        }

        dir /= len;

        const float3 up = { 0.0f, 1.0f, 0.0f };
        float3 side = normalize(cross(dir, up)) * width;

        const uint32_t base = static_cast<uint32_t>(vertices.size());

        vertices.push_back(a - side);
        vertices.push_back(a + side);
        vertices.push_back(b + side);
        vertices.push_back(b - side);

        indices.insert(indices.end(),
                { base + 0, base + 1, base + 2, base + 2, base + 3, base + 0 });
    };

    for (int i = -n; i <= n; ++i) {
        const float p = i * spacing;
        const bool center = i == 0;
        const bool major = (i % 10) == 0;

        if (center) {
            addLineQuadTo(centerLineVerts, centerLineIdx, { -extent, centerLineY, p },
                    { extent, centerLineY, p }, centerW);

            addLineQuadTo(centerLineVerts, centerLineIdx, { p, centerLineY, -extent },
                    { p, centerLineY, extent }, centerW);
        } else {
            const float width = major ? w * 2.0f : w;

            addLineQuadTo(lineVerts, lineIdx, { -extent, lineY, p }, { extent, lineY, p }, width);

            addLineQuadTo(lineVerts, lineIdx, { p, lineY, -extent }, { p, lineY, extent }, width);
        }
    }

    buildRenderable(lineVerts, lineIdx, lineMaterial);
    buildRenderable(centerLineVerts, centerLineIdx, centerLineMaterial);

    return true;
}

void WorldGrid::setVisible(Engine* engine, bool visible) {
    auto& rcm = engine->getRenderableManager();

    for (utils::Entity entity: mEntities) {
        auto inst = rcm.getInstance(entity);
        if (inst) {
            rcm.setLayerMask(inst, 0xFF, visible ? 0x01 : 0x00);
        }
    }

    mVisible = visible;
}

void WorldGrid::destroy(Engine* engine) {
    for (utils::Entity entity: mEntities) {
        if (entity) {
            engine->destroy(entity);
            utils::EntityManager::get().destroy(entity);
        }
    }

    for (VertexBuffer* vb: mVertexBuffers) {
        if (vb) {
            engine->destroy(vb);
        }
    }

    for (IndexBuffer* ib: mIndexBuffers) {
        if (ib) {
            engine->destroy(ib);
        }
    }

    for (MaterialInstance* mi: mMaterialInstances) {
        if (mi) {
            engine->destroy(mi);
        }
    }

    for (Material* material: mMaterials) {
        if (material) {
            engine->destroy(material);
        }
    }

    mEntities.clear();
    mVertexBuffers.clear();
    mIndexBuffers.clear();
    mMaterialInstances.clear();
    mMaterials.clear();

    delete this;
}
