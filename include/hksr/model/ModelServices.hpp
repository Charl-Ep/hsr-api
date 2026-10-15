#pragma once
#include "../Export.hpp"
#include <memory>

namespace hksr::sdk_v1 {
struct ModelServiceBindings;
struct ManagedObjects;
struct ObjectResult;
template<class T> struct Observed;
class EntityAccess;
class ModelAccess;
class RendererAccess;
class ArtAdmission;
class ManagedRoots;
class AssetLeases;
class OwnedUnityObjects;
class LookSupport;

// Native capability provider. Each factory returns a separate explicit owner;
// constructing or destroying this provider performs no engine operations.
// Borrowed accessors remain valid while this provider lives. Close every owner
// on its engine thread before destroying it; factories do not merge lifetimes.
class HKSR_API ModelServices {
public:
    explicit ModelServices(ModelServiceBindings);
    ~ModelServices();
    ModelServices(const ModelServices&)=delete;
    ModelServices& operator=(const ModelServices&)=delete;
    const EntityAccess& Entities() const;
    const ModelAccess& Models() const;
    const RendererAccess& Renderers() const;
    const ArtAdmission& Admission() const;
    std::unique_ptr<ManagedRoots> CreateRoots() const;
    std::unique_ptr<AssetLeases> CreateAssetLeases() const;
    std::unique_ptr<OwnedUnityObjects> CreateNativeObjects() const;
    std::unique_ptr<LookSupport> CreateLookSupport() const;
    ObjectResult ComponentType(ManagedObjects&) const;
    Observed<bool> Alive(void* rooted_object) const;
    bool CanOverrideMeshAndMaterials() const;
    bool CanOverrideBones() const;
private:
    struct State;
    State* state_;
};
}
