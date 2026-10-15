#include <hksr/presentation/PresentationResources.hpp>
#include <hksr/presentation/PresentationBindings.hpp>

namespace hksr::sdk_v1 {
struct PresentationResources::State {
    MeshAccess mesh;
    MeshSkinning skinning;
    MeshTemplateAccess templates;
    TextureAccess texture;
    MaterialAccess material;
    ShaderAccess shader;
    ManagedArrays arrays;
    HierarchyAccess hierarchy;
    SceneObjects scene;
    VisualObjects visuals;
    AdventureConfig config;
    ComponentConfig components;
    explicit State(const PresentationBindings& b)
        :mesh(b.mesh),skinning(b.skinning),templates(b.mesh_templates),texture(b.texture),
        material(b.material),shader(b.shader),arrays(b.arrays),hierarchy(b.hierarchy),
        scene(b.scene),visuals(b.visual),config(b.config),components(b.components) {}
};
PresentationResources::PresentationResources(const PresentationBindings& b):state_(new State(b)){}
PresentationResources::~PresentationResources(){delete state_;}
const MeshAccess& PresentationResources::Mesh() const{return state_->mesh;}
const MeshSkinning& PresentationResources::Skinning() const{return state_->skinning;}
const MeshTemplateAccess& PresentationResources::MeshTemplates() const{return state_->templates;}
const TextureAccess& PresentationResources::Texture() const{return state_->texture;}
const MaterialAccess& PresentationResources::Material() const{return state_->material;}
const ShaderAccess& PresentationResources::Shader() const{return state_->shader;}
const ManagedArrays& PresentationResources::Arrays() const{return state_->arrays;}
const HierarchyAccess& PresentationResources::Hierarchy() const{return state_->hierarchy;}
const SceneObjects& PresentationResources::Scene() const{return state_->scene;}
const VisualObjects& PresentationResources::Visuals() const{return state_->visuals;}
const AdventureConfig& PresentationResources::Config() const{return state_->config;}
const ComponentConfig& PresentationResources::Components() const{return state_->components;}
}
