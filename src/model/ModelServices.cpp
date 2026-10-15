#include <hksr/model/ModelServices.hpp>
#include <hksr/model/ModelServiceBindings.hpp>
#include <hksr/managed/ManagedObjects.hpp>
#include <hksr/unity/AssetLeases.hpp>
#include <hksr/control/LookSupport.hpp>

namespace hksr::sdk_v1 {
struct ModelServices::State {
    ModelServiceBindings bindings;
    EntityAccess entities;
    RendererAccess renderers;
    explicit State(ModelServiceBindings input)
        :bindings(std::move(input)),entities(bindings.entities),renderers(bindings.renderers){}
};
ModelServices::ModelServices(ModelServiceBindings bindings):state_(new State(std::move(bindings))){}
ModelServices::~ModelServices(){delete state_;}
const EntityAccess& ModelServices::Entities() const{return state_->entities;}
const ModelAccess& ModelServices::Models() const{return state_->bindings.models;}
const RendererAccess& ModelServices::Renderers() const{return state_->renderers;}
const ArtAdmission& ModelServices::Admission() const{return state_->bindings.admission;}
std::unique_ptr<ManagedRoots> ModelServices::CreateRoots() const {
    return std::make_unique<ManagedRoots>(state_->bindings.references);
}
std::unique_ptr<AssetLeases> ModelServices::CreateAssetLeases() const {
    return std::make_unique<AssetLeases>(state_->bindings.references);
}
std::unique_ptr<OwnedUnityObjects> ModelServices::CreateNativeObjects() const {
    return std::make_unique<OwnedUnityObjects>(state_->bindings.lifetime,state_->bindings.references);
}
std::unique_ptr<LookSupport> ModelServices::CreateLookSupport() const {
    return std::make_unique<LookSupport>(state_->bindings.references,state_->bindings.lifetime);
}
ObjectResult ModelServices::ComponentType(ManagedObjects& objects) const {
    return objects.ManagedType("RPG.GameCore","CharacterModelComponent");
}
Observed<bool> ModelServices::Alive(void* object) const {
    return UnityObjectAlive(state_->bindings.lifetime.alive,object);
}
bool ModelServices::CanOverrideMeshAndMaterials() const {
    const auto& r=state_->bindings.renderers;
    return r.get_mesh.code && r.set_mesh.code && r.get_materials.code && r.set_materials.code;
}
bool ModelServices::CanOverrideBones() const {
    const auto& r=state_->bindings.renderers;
    return r.get_bones.code && r.set_bones.code;
}
}
