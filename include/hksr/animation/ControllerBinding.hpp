#pragma once
#include <string>
#include <vector>
#include <hksr/animation/AnimationControl.hpp>
#include <hksr/unity/AssetLeases.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedObjects.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/unity/SceneObjects.hpp>
#include <hksr/world/WorldFrames.hpp>

namespace hksr::sdk_v1 {
struct ControllerBindingResult {bool complete=false;std::vector<std::string> errors;};
enum class ControllerSourceKind { ResourcePath, PrefabAnimator };
struct ControllerSource { ControllerSourceKind kind; std::string path; };
struct ControllerBindingServices {
    AnimationControl animation;
    EntityAccess entities;
    SceneObjects scene;
    ManagedArrays arrays;
    ControllerLinkField model_loader,loader_model,loader_manager;
};
struct ControllerBindingStatus {
    bool started=false,submitted=false,closing=false,detached=true,closed=false;
    CallResult setup,apply,remove;
    bool remove_attempted=false;
    AssetLeaseId lease=0;
    AssetLeaseStatus source_lease;
    void* controller=nullptr;
    void* loader=nullptr;
    void* manager=nullptr;
    ControllerBindingResult preparation,cleanup;
};
// One override on one owned model. Independent Prefab lease, no scene/player
// donor, no controller mutation/clone/Destroy. Apply/Close on the engine thread;
// caller closes BEFORE replacing the model. Destruction makes no engine calls.
// apply.returned is not proof of Animator adoption or state availability.
class HKSR_API ControllerBinding {
public:
    ControllerBinding(EntityToken owner,GameWorld* world,
        ReferenceMethods references,ControllerBindingServices services);
    ~ControllerBinding();
    ControllerBinding(const ControllerBinding&) = delete;
    ControllerBinding& operator=(const ControllerBinding&) = delete;
    ControllerBindingResult Apply(const WorldFrame& frame,CharacterModelComponent* model,
        ManagedObjects& objects,const ControllerSource& source);
    ControllerBindingResult Close(const WorldFrame& frame);
    const ControllerBindingStatus& Status() const;
private:
    struct State;
    State* state_;
};
}
