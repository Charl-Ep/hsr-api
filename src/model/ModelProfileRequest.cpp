#include <hksr/model/ModelProfileRequest.hpp>
#include <hksr/model/ModelVisibilityLease.hpp>
#include <stdexcept>
#include <algorithm>

namespace hksr::sdk_v1 {
struct ModelProfileRequest::State {
    State(EntityToken owner,GameWorld* world,
        std::unique_ptr<ModelProfileInputs> inputs,ReferenceMethods refs,ModelRequestServices services);
    void Submit(const WorldFrame& frame,CharacterModelComponent* current_model,ModelDestination destination);
    void Observe(const WorldFrame& frame,CharacterModelComponent* current_model,bool require_animator);
    // Use the current model's actual event component and consumer declarations,
    // not those captured before model callbacks reset its event inputs.
    void BindEvents(const WorldFrame& frame,CharacterModelComponent* current_model,
        void* current_event_component,ProfileEventFields fields);
    bool Cancel(const WorldFrame& frame);
    ModelProfileResult Close(const WorldFrame& frame);
    bool LiveFrame(const WorldFrame& frame);
    bool Current(const WorldFrame& frame,CharacterModelComponent* model);
    bool CancelOperation();
    bool Matches(void* a,void* b);
    void* Keep(void* value);
    void Fail(const char* message);
    const ModelInputs& Selected() const;
    EntityToken owner_;
    GameWorld* world_;
    std::unique_ptr<ModelProfileInputs> inputs_;
    ModelRequestServices services_;
    ManagedRoots roots_;
    std::vector<ManagedRootId> handles_;
    ManagedRootId expected_root_=0;
    void* expected_path_=nullptr;
    ModelRequestStatus status_;
    std::unique_ptr<EventValueLease> values_;
    std::unique_ptr<ModelVisibilityLease> visibility_;
    bool values_ready_=false;
    std::vector<std::unique_ptr<EventValueLease>> retired_values_;
};
ModelProfileRequest::State::State(EntityToken owner,GameWorld* world,
    std::unique_ptr<ModelProfileInputs> inputs,ReferenceMethods refs,ModelRequestServices services)
    :owner_(owner),world_(world),inputs_(std::move(inputs)),services_(std::move(services)),roots_(refs) {
    if(!owner.entity || !owner.generation || !world || !inputs_ || !inputs_->Status().prepared)
        throw std::invalid_argument("model request needs an owned epoch and prepared inputs");
    try { Keep(owner.entity); }
    catch(const std::exception& e) { Fail(e.what()); }
}
void* ModelProfileRequest::State::Keep(void* value) {
    if(!value) throw std::runtime_error("model request reference missing");
    handles_.push_back(0); const auto id=roots_.Keep(value); handles_.back()=id;
    if(!roots_.Status(id).ready) throw std::runtime_error("model request reference retention failed");
    return value;
}
void ModelProfileRequest::State::Fail(const char* message) {
    status_.phase=ModelRequestPhase::Failed; status_.errors={message};
}
bool ModelProfileRequest::State::LiveFrame(const WorldFrame& frame) {
    if(status_.closed) return false;
    if(TrackedEntities().IsDisposed(owner_) || TrackedEntities().Current(owner_.entity).generation!=owner_.generation) {
        status_.phase=ModelRequestPhase::OwnerEnded; status_.operation_in_use=false; return false;
    }
    return frame.world==world_ && frame.disposing.read.returned && !frame.disposing.value;
}
bool ModelProfileRequest::State::Current(const WorldFrame& frame,CharacterModelComponent* model) {
    if(!LiveFrame(frame)) return false;
    if(model!=inputs_->Status().model) { Fail("owned model component replaced; this request cannot write the replacement"); return false; }
    return true;
}
bool ModelProfileRequest::State::Matches(void* a,void* b) {
    if(a==b) return true;
    if(!a || !b) return false;
    const auto equal=services_.strings.EqualNames(a,b);
    if(!equal.read.returned) throw std::runtime_error("official model path comparison unavailable");
    return equal.value;
}
const ModelInputs& ModelProfileRequest::State::Selected() const {
    return status_.destination==ModelDestination::Baseline?inputs_->Status().baseline:inputs_->Status().target;
}
bool ModelProfileRequest::State::CancelOperation() {
    if(!status_.operation_in_use) return true;
    if(!status_.cancel_attempted) {
        status_.cancel_attempted=true;
        status_.cancel=services_.profile.Cancel(inputs_->Status().operation.object);
    }
    if(!status_.cancel.returned) { Fail("official model operation cancellation unresolved"); return false; }
    status_.operation_in_use=false;
    return true;
}
void ModelProfileRequest::State::Submit(const WorldFrame& frame,CharacterModelComponent* model,ModelDestination destination) {
    if(!Current(frame,model) || !CancelOperation()) return;
    if(!Current(frame,model)) return; // cancellation can synchronously end the owner
    // A failed owner retention cannot be bypassed by a later user command.
    if(handles_.empty() || !roots_.Status(handles_.front()).ready) { Fail("model request owner retention unavailable"); return; }
    ++status_.serial; status_.destination=destination; status_.phase=ModelRequestPhase::Idle;
    status_.errors.clear(); status_.resolution={}; status_.submit={}; status_.cancel={};
    status_.cancel_attempted=false; status_.events_attempted=false; status_.event_replace={};
    status_.observation={}; status_.completed={}; status_.animator_initialized={};
    try {
        const auto& input=Selected(); void* resolved=nullptr;
        if(visibility_ && !visibility_->Select(owner_,model,destination==ModelDestination::Alternate))
            throw std::runtime_error(visibility_->Error());
        status_.resolution=services_.profile.CanChange(model,input.model_path,&resolved);
        if(!status_.resolution.call.returned) throw std::runtime_error("official target path resolution failed");
        auto* expected=input.model_path;
        if(status_.resolution.changed) {
            if(!resolved) throw std::runtime_error("official target resolution returned no path");
            expected=resolved;
        } else {
            const auto actual=services_.inputs.ModelPath(model);
            if(!actual.call.returned || !Matches(actual.object,input.model_path))
                throw std::runtime_error("model request declined for a different resource");
        }
        // Old expected roots are GC-only and cannot block the next operation.
        if(expected_root_ && roots_.Release(expected_root_).complete) {
            roots_.Forget(expected_root_);
            handles_.erase(std::remove(handles_.begin(),handles_.end(),expected_root_),handles_.end());
        }
        expected_root_=0; expected_path_=nullptr;
        expected_path_=Keep(expected); expected_root_=handles_.back();
        if(destination==ModelDestination::Baseline && inputs_->Status().baseline_entity_config) {
            status_.submit=services_.profile.Restore(model,input.model_path,inputs_->Status().baseline_entity_config);
        } else {
            status_.operation_in_use=true; // Begin may mutate before throwing.
            status_.submit=services_.profile.Begin(inputs_->Status().operation.object,owner_.entity,input,false,nullptr);
        }
        if(!status_.submit.returned) throw std::runtime_error("official model submission failed; model may already have changed");
        if(!Current(frame,model)) return; // do not read an entity ended by a callback
        const auto actual=services_.inputs.ModelPath(model);
        if(!actual.call.returned || !Matches(actual.object,expected_path_))
            throw std::runtime_error("submitted model differs from the official resolved target");
        if(destination==ModelDestination::Baseline && inputs_->Status().baseline_entity_config) {
            const auto config=services_.inputs.EntityConfig(model);
            if(!config.call.returned || !Matches(config.object,inputs_->Status().baseline_entity_config))
                throw std::runtime_error("original entity configuration was not restored");
        }
        status_.phase=ModelRequestPhase::AwaitingModel;
    } catch(const std::exception& e) { Fail(e.what()); }
}
void ModelProfileRequest::State::Observe(const WorldFrame& frame,CharacterModelComponent* model,bool require_animator) {
    if(!Current(frame,model) || status_.phase!=ModelRequestPhase::AwaitingModel) return;
    try {
        const auto path=services_.inputs.ModelPath(model);
        if(!path.call.returned || !Matches(path.object,expected_path_)) throw std::runtime_error("model path no longer belongs to submitted request");
        status_.observation=services_.model.Read(model);
        const auto& state=status_.observation;
        if(!services_.completion) throw std::runtime_error("model load completion contract unavailable");
        status_.completed=services_.completion(state);
        if(!status_.completed.read.returned || !state.root.read.returned || !state.changing.read.returned)
            throw std::runtime_error("model load observation failed");
        if(!status_.completed.value || !state.root.value || state.changing.value) return;
        // Returning to a private exploration Art can intentionally have no
        // Animator yet. Do not wait for the object that Adopt will create.
        if(require_animator) {
            if(!state.animator.read.returned) throw std::runtime_error("loaded model Animator query failed");
            if(!state.animator.value) return;
            status_.animator_initialized=services_.inputs.AnimatorInitialized(state.animator.value);
            if(!status_.animator_initialized.read.returned) throw std::runtime_error("Animator initialization query failed");
            if(!status_.animator_initialized.value) return;
        }
        status_.phase=ModelRequestPhase::ModelReady;
    } catch(const std::exception& e) { Fail(e.what()); }
}
void ModelProfileRequest::State::BindEvents(const WorldFrame& frame,CharacterModelComponent* model,
    void* component,ProfileEventFields fields) {
    if(!Current(frame,model) || status_.phase!=ModelRequestPhase::ModelReady || status_.events_attempted) return;
    try {
        const auto path=services_.inputs.ModelPath(model);
        if(!path.call.returned || !Matches(path.object,expected_path_)) throw std::runtime_error("model changed before event adoption");
        if(!component) throw std::runtime_error("current model event component unavailable");
        for(auto it=retired_values_.begin();it!=retired_values_.end();) {
            if((*it)->Close(false))it=retired_values_.erase(it);else ++it;
        }
        if(status_.destination==ModelDestination::Alternate && !inputs_->Status().character_config.empty() && !values_) {
            if(!services_.event_values)throw std::runtime_error("event value input service unavailable");
            values_=services_.event_values();
            if(!values_ || !values_->Apply(component,inputs_->Status().character_config))
                throw std::runtime_error(values_?values_->Error():"event value input allocation failed");
            values_ready_=true;
            status_.event_value_note="private numeric/string defaults adopted: "+inputs_->Status().character_config;
            if(!Current(frame,model))return;
        }
        if(status_.destination==ModelDestination::Alternate && values_ && !values_ready_)
            throw std::runtime_error(values_->Error());
        const auto before=services_.events.Consumer(component);
        if(!before.call.returned || !before.object) throw std::runtime_error("current model event consumer unavailable");
        const auto arrays=ReadProfileEventArrays(before.object,fields);
        const auto& input=Selected();
        const bool installed=arrays.paths.call.returned && arrays.layers.call.returned &&
            arrays.paths.object==input.event_paths && arrays.layers.object==input.excluded_layers;
        status_.events_attempted=true;
        if(!installed) {
            status_.event_replace=services_.profile.ReplaceEvents(component,input.event_paths,input.excluded_layers);
            if(!status_.event_replace.returned) throw std::runtime_error("official event input adoption failed");
            if(!Current(frame,model)) return;
        }
        const auto after=services_.events.Consumer(component);
        if(!after.call.returned || !after.object) throw std::runtime_error("event consumer unavailable after adoption");
        const auto result=ReadProfileEventArrays(after.object,fields);
        if(!result.paths.call.returned || !result.layers.call.returned ||
            result.paths.object!=input.event_paths || result.layers.object!=input.excluded_layers)
            throw std::runtime_error("event call returned but requested arrays were not installed");
        status_.phase=ModelRequestPhase::EventsReady;
        if(status_.destination==ModelDestination::Baseline && values_) {
            // Keep the battle tables through the official model/event exit.
            // A completed restore can leave only GC cleanup, not block NPC use.
            if(!values_->Restore())throw std::runtime_error(values_->Error());
            retired_values_.push_back(std::move(values_));
            values_ready_=false;
            status_.event_value_note="original numeric/string tables restored";
        }
    } catch(const std::exception& e) { Fail(e.what()); }
}
bool ModelProfileRequest::State::Cancel(const WorldFrame& frame) {
    if(!LiveFrame(frame)) return status_.phase==ModelRequestPhase::OwnerEnded;
    if(!CancelOperation()) return false;
    if(!LiveFrame(frame)) return status_.phase==ModelRequestPhase::OwnerEnded;
    status_.phase=ModelRequestPhase::Cancelled; return true;
}
ModelProfileResult ModelProfileRequest::State::Close(const WorldFrame& frame) {
    ModelProfileResult out;
    if(status_.closed) { out.complete=true; return out; }
    if(!Cancel(frame)) { out.errors.push_back("model request still in use or outside its owner frame"); return out; }
    const bool ended=status_.phase==ModelRequestPhase::OwnerEnded;
    if(visibility_ && !visibility_->Close())out.errors.push_back("weapon visibility input roots pending release");
    if(values_ && !values_->Close(ended))out.errors.push_back(values_->Error().empty()?"event value roots pending release":values_->Error());
    for(auto& old:retired_values_)if(!old->Close(ended))out.errors.push_back("retired event value roots pending release");
    if(!out.errors.empty())return out;
    if(values_ || !retired_values_.empty())status_.event_value_note=ended?"owner ended; value roots released without writes":"value tables restored and roots released";
    out=inputs_->Close();
    for(auto id:handles_) if(id && !roots_.Release(id).complete) out.errors.push_back("model request root release unresolved");
    out.complete=out.errors.empty(); status_.closed=out.complete; return out;
}

ModelProfileRequest::ModelProfileRequest(EntityToken owner,GameWorld* world,std::unique_ptr<ModelProfileInputs> inputs,
    ReferenceMethods refs,ModelRequestServices services)
    :state_(new State(owner,world,std::move(inputs),refs,std::move(services))) {}
ModelProfileRequest::~ModelProfileRequest() { delete state_; }
void ModelProfileRequest::SetVisibilityLease(std::unique_ptr<ModelVisibilityLease> visibility) {
    if(state_->status_.serial || state_->visibility_)throw std::logic_error("model visibility lease already configured");
    state_->visibility_=std::move(visibility);
}
const ModelRequestStatus& ModelProfileRequest::Status() const { return state_->status_; }
void ModelProfileRequest::Submit(const WorldFrame& frame,CharacterModelComponent* model,ModelDestination destination) {
    state_->Submit(frame,model,destination);
}
void ModelProfileRequest::Observe(const WorldFrame& frame,CharacterModelComponent* model,bool require_animator) {
    state_->Observe(frame,model,require_animator);
}
void ModelProfileRequest::BindEvents(const WorldFrame& frame,CharacterModelComponent* model,void* component,ProfileEventFields fields) {
    state_->BindEvents(frame,model,component,fields);
}
bool ModelProfileRequest::Cancel(const WorldFrame& frame) { return state_->Cancel(frame); }
ModelProfileResult ModelProfileRequest::Close(const WorldFrame& frame) { return state_->Close(frame); }
}
