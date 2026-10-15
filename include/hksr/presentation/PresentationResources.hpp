#pragma once
#include "../Export.hpp"

namespace hksr::sdk_v1 {
struct PresentationBindings;
class MeshAccess;
class MeshSkinning;
class MeshTemplateAccess;
class TextureAccess;
class MaterialAccess;
class ShaderAccess;
class ManagedArrays;
class HierarchyAccess;
class SceneObjects;
class VisualObjects;
class AdventureConfig;
class ComponentConfig;

// Native resource services owned by PresentationContext. Accessors borrow
// SDK-owned wrappers; they do not allocate managed objects or load assets.
class HKSR_API PresentationResources {
public:
    explicit PresentationResources(const PresentationBindings&); // low-level composition
    ~PresentationResources();
    PresentationResources(const PresentationResources&)=delete;
    PresentationResources& operator=(const PresentationResources&)=delete;
    const MeshAccess& Mesh() const;
    const MeshSkinning& Skinning() const;
    const MeshTemplateAccess& MeshTemplates() const;
    const TextureAccess& Texture() const;
    const MaterialAccess& Material() const;
    const ShaderAccess& Shader() const;
    const ManagedArrays& Arrays() const;
    const HierarchyAccess& Hierarchy() const;
    const SceneObjects& Scene() const;
    const VisualObjects& Visuals() const;
    const AdventureConfig& Config() const;
    const ComponentConfig& Components() const;
private:
    struct State;
    State* state_;
};
}
