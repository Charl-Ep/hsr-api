#include <hksr/animation/ControllerBinding.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <atomic>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace { std::atomic<std::uint64_t> sequence{0}; }
struct ControllerBinding::State {
    State(EntityToken owner,GameWorld* world,ReferenceMethods references,ControllerBindingServices services);
    ControllerBindingResult Apply(const WorldFrame& frame,CharacterModelComponent* model,
        ManagedObjects& objects,const ControllerSource& source);
    ControllerBindingResult Close(const WorldFrame& frame);
    bool Ended() const;
    bool OwnFrame(const WorldFrame& frame) const;
    bool CurrentModel() const;
    void* Keep(ObjectResult result,const char* label);
    bool SameLoader() const;
    EntityToken owner_;
    GameWorld* world_;
    CharacterModelComponent* model_=nullptr;
    void* model_type_=nullptr;
    void* tag_=nullptr;
    ControllerSourceKind kind_=ControllerSourceKind::ResourcePath;
    ControllerBindingServices services_;
    ManagedRoots roots_;
    AssetLeases leases_;
    std::vector<ManagedRootId> handles_;
    bool busy_=false;
    ControllerBindingStatus status_;
};
ControllerBinding::State::State(EntityToken owner,GameWorld* world,
    ReferenceMethods refs,ControllerBindingServices services)
    :owner_(owner),world_(world),services_(std::move(services)),roots_(refs),leases_(refs) {
    if(!owner.entity || !owner.generation || !world) throw std::invalid_argument("controller binding requires owned entity epoch");
}
bool ControllerBinding::State::Ended() const {
    return TrackedEntities().IsDisposed(owner_) || TrackedEntities().Current(owner_.entity).generation!=owner_.generation;
}
bool ControllerBinding::State::OwnFrame(const WorldFrame& frame) const {
    return frame.world==world_ && frame.disposing.read.returned && !frame.disposing.value;
}
bool ControllerBinding::State::CurrentModel() const {
    if(Ended()) return false;
    const auto entity=services_.entities.Read(owner_.entity);
    if(Ended() || !entity.owner_world.read.returned || entity.owner_world.value!=world_) return false;
    const auto component=services_.entities.Component(owner_.entity,model_type_);
    return !Ended() && component.call.returned && component.object==model_;
}
void* ControllerBinding::State::Keep(ObjectResult value,const char* label) {
    if(!value.call.returned || !value.object) throw std::runtime_error(label);
    const auto id=roots_.Keep(value.object); handles_.push_back(id);
    if(!roots_.Status(id).ready) throw std::runtime_error(std::string(label)+": retain failed");
    return value.object;
}
bool ControllerBinding::State::SameLoader() const {
    const auto loader=ReadControllerLink(model_,services_.model_loader);
    if(!loader.call.returned || loader.object!=status_.loader) return false;
    const auto model=ReadControllerLink(status_.loader,services_.loader_model);
    const auto manager=ReadControllerLink(status_.loader,services_.loader_manager);
    return model.call.returned && model.object==model_ && manager.call.returned && manager.object==status_.manager;
}
ControllerBindingResult ControllerBinding::State::Apply(const WorldFrame& frame,CharacterModelComponent* model,
    ManagedObjects& objects,const ControllerSource& source) {
    if(status_.started || status_.closing) throw std::logic_error("controller binding is single-use");
    if(!OwnFrame(frame) || Ended()) return {false,{"controller apply requires live owning world frame"}};
    status_.started=true; busy_=true;
    struct Exit { bool& flag; ~Exit(){flag=false;} } exit{busy_};
    try {
        if(source.path.empty() || source.path.find('\0')!=std::string::npos) throw std::invalid_argument("invalid controller source path");
        kind_=source.kind; model_=model;
        Keep({{true,0},owner_.entity},"controller owner");
        model_type_=Keep(objects.ManagedType("RPG.GameCore","CharacterModelComponent"),"model Type");
        if(!model || !CurrentModel()) throw std::runtime_error("controller model is not current owned component");
        Keep({{true,0},model},"controller model");
        auto* path=Keep(objects.NewStringUtf8(source.path.c_str()),"controller source path");
        if(kind_==ControllerSourceKind::PrefabAnimator) {
            auto* type=Keep(objects.ManagedType("UnityEngine","GameObject"),"controller prefab Type");
            status_.lease=leases_.Acquire(path,type,false);
            status_.source_lease=leases_.Status(status_.lease);
            auto* prefab=leases_.Object(status_.lease);
            if(!prefab) throw std::runtime_error("controller prefab lease unavailable");
            auto* animator_type=Keep(objects.ManagedType("UnityEngine","Animator"),"source Animator Type");
            auto* array=Keep(services_.scene.Components(prefab,animator_type,true),"source Animator inventory");
            const auto count=services_.arrays.Length(array);
            if(!count.read.returned || count.value!=1) throw std::runtime_error("explicit controller prefab requires a unique Animator");
            auto* animator=Keep(services_.arrays.Get(array,0),"source Animator");
            status_.controller=Keep(services_.animation.Controller(animator),"source controller");
            const auto tag="Hksr/Controller/"+std::to_string(++sequence);
            tag_=Keep(objects.NewStringUtf8(tag.c_str()),"owned controller tag");
            if(!services_.model_loader.available || !services_.loader_model.available || !services_.loader_manager.available)
                throw std::runtime_error("controller loader links unavailable");
            if(!CurrentModel() || status_.closing) throw std::runtime_error("controller target ended or apply cancelled");
            // Retained official empty-path setup creates the loader without a
            // failed asset lookup. This class exclusively owns its default key.
            status_.setup=services_.animation.ReplacePath(model_,nullptr);
            if(!status_.setup.returned || !CurrentModel() || status_.closing) throw std::runtime_error("controller loader setup failed or cancelled");
            status_.loader=Keep(ReadControllerLink(model_,services_.model_loader),"owned loader");
            const auto back=ReadControllerLink(status_.loader,services_.loader_model);
            if(!back.call.returned || back.object!=model_) throw std::runtime_error("controller loader does not refer to owned model");
            status_.manager=Keep(services_.animation.OverrideManager(status_.loader),"owned override manager");
            if(!CurrentModel() || status_.closing || !SameLoader()) throw std::runtime_error("controller target changed before submission");
        } else if(!CurrentModel() || status_.closing) throw std::runtime_error("controller target ended or apply cancelled");
        status_.submitted=true; status_.detached=false; // before possibly partial side effect
        status_.apply=kind_==ControllerSourceKind::PrefabAnimator?
            services_.animation.ReplaceObject(status_.manager,status_.controller,tag_):
            services_.animation.ReplacePath(model_,path);
        if(!status_.apply.returned) throw std::runtime_error("controller application did not return");
        status_.preparation.complete=!Ended() && !status_.closing;
        if(!status_.preparation.complete) throw std::runtime_error("controller owner ended or close requested during apply");
    } catch(const std::exception& error) { status_.preparation.errors.push_back(error.what()); }
    return status_.preparation;
}
ControllerBindingResult ControllerBinding::State::Close(const WorldFrame& frame) {
    status_.closing=true;
    if(status_.closed) return status_.cleanup;
    if(busy_) return {false,{"controller call in flight"}};
    if(!Ended() && !OwnFrame(frame)) return {false,{"controller close waits for owning world frame"}};
    busy_=true; struct Exit { bool& flag; ~Exit(){flag=false;} } exit{busy_};
    ControllerBindingResult out;
    if(Ended()) status_.detached=true; // actual entity end, not failed Apply
    if(!status_.detached) {
        if(!CurrentModel() || (kind_==ControllerSourceKind::PrefabAnimator && !SameLoader()))
            out.errors.push_back("controller owner binding changed; no late write to old target");
        else if(!status_.remove_attempted) {
            status_.remove_attempted=true;
            status_.remove=kind_==ControllerSourceKind::PrefabAnimator?
                services_.animation.ReplaceObject(status_.manager,nullptr,tag_):services_.animation.ReplacePath(model_,nullptr);
            status_.detached=status_.remove.returned || Ended();
        }
    }
    if(status_.detached) {
        if(status_.lease) {
            status_.source_lease=leases_.Release(status_.lease);
            if(!status_.source_lease.complete) out.errors.push_back("controller source lease cleanup unresolved");
        }
        for(auto id:handles_) if(!roots_.Release(id).complete) out.errors.push_back("controller input root cleanup unresolved");
    } else out.errors.push_back("controller override removal unresolved");
    out.complete=out.errors.empty(); status_.cleanup=out; status_.closed=out.complete; return out;
}
ControllerBinding::ControllerBinding(EntityToken owner,GameWorld* world,
    ReferenceMethods refs,ControllerBindingServices services)
    :state_(new State(owner,world,refs,std::move(services))) {}
ControllerBinding::~ControllerBinding(){delete state_;}
ControllerBindingResult ControllerBinding::Apply(const WorldFrame& frame,CharacterModelComponent* model,
    ManagedObjects& objects,const ControllerSource& source){return state_->Apply(frame,model,objects,source);}
ControllerBindingResult ControllerBinding::Close(const WorldFrame& frame){return state_->Close(frame);}
const ControllerBindingStatus& ControllerBinding::Status() const{return state_->status_;}
}
