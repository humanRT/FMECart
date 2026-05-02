#pragma once

#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Scene.h>
#include <filament/VertexBuffer.h>

#include <math/vec4.h>
#include <utils/Entity.h>

#include <vector>

class WorldGrid {
public:
    static WorldGrid* create(filament::Engine* engine, filament::Scene* scene,
            float halfSize = 5.0f, float spacing = 0.10f, float y = 0.0f,
            float thickness = 0.0025f);

    void setVisible(filament::Engine* engine, bool visible);
    bool isVisible() const { return mVisible; }

    void destroy(filament::Engine* engine);

private:
    WorldGrid() = default;

    bool build(filament::Engine* engine, filament::Scene* scene, float halfSize, float spacing,
            float y, float thickness);

    filament::MaterialInstance* createMaterial(filament::Engine* engine,
            const filament::math::float4& color);

private:
    std::vector<utils::Entity> mEntities;

    std::vector<filament::VertexBuffer*> mVertexBuffers;
    std::vector<filament::IndexBuffer*> mIndexBuffers;
    std::vector<filament::Material*> mMaterials;
    std::vector<filament::MaterialInstance*> mMaterialInstances;

    bool mVisible = true;
};
