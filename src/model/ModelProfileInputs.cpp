#include <hksr/model/ModelProfileInputs.hpp>
#include <limits>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct ModelProfileInputs::State {
    explicit State(ReferenceMethods refs):roots_(refs){}
    void Prepare(ManagedObjects& objects,const ManagedArrays& arrays,const ProfileInputs& access,
        ProfileEventFields event_fields,void* owned_model,void* owned_event_consumer,const ProfileDescription& target);
    ModelProfileResult Close();
    void* Keep(ObjectResult object,bool nullable=false);
    void* String(ManagedObjects& objects,const std::string& text);
    void* Strings(ManagedObjects& objects,const ManagedArrays& arrays,void* string_type,const std::vector<std::string>& values);
    ManagedRoots roots_;
    std::vector<ManagedRootId> handles_;
    ModelProfileInputStatus status_;
};
void* ModelProfileInputs::State::Keep(ObjectResult value,bool nullable) {
    if(!value.call.returned || (!nullable && !value.object)) throw std::runtime_error("model profile input unavailable");
    if(!value.object) return nullptr;
    handles_.push_back(0); auto id=roots_.Keep(value.object); handles_.back()=id;
    if(!roots_.Status(id).ready) throw std::runtime_error("model profile input retention failed");
    return value.object;
}
void* ModelProfileInputs::State::String(ManagedObjects& objects,const std::string& text) {
    return Keep(objects.NewStringUtf8(text.c_str()));
}
void* ModelProfileInputs::State::Strings(ManagedObjects& objects,const ManagedArrays& arrays,
    void* string_type,const std::vector<std::string>& values) {
    if(values.size()>static_cast<std::size_t>((std::numeric_limits<int>::max)())) throw std::overflow_error("official array length exceeds Int32 ABI");
    auto* array=Keep(arrays.Create(string_type,static_cast<int>(values.size())));
    for(std::size_t i=0;i<values.size();++i)
        if(!arrays.Set(array,String(objects,values[i]),static_cast<int>(i)).returned)
            throw std::runtime_error("model profile array element assignment failed");
    return array;
}
void ModelProfileInputs::State::Prepare(ManagedObjects& objects,const ManagedArrays& arrays,
    const ProfileInputs& access,ProfileEventFields fields,void* model,void* consumer,const ProfileDescription& target) {
    if(status_.started || status_.closed) throw std::logic_error("model profile inputs are single-use");
    status_.started=true;
    try {
        if(!model || !consumer || target.model_path.empty()) throw std::invalid_argument("profile needs owned model, consumer and target source");
        status_.model=Keep({{true,0},model}); Keep({{true,0},consumer});
        status_.character_config=target.character_config;
        status_.baseline.model_path=Keep(access.ModelPath(model));
        status_.baseline_entity_config=Keep(access.EntityConfig(model),true);
        const auto events=ReadProfileEventArrays(consumer,fields);
        status_.baseline.event_paths=Keep(events.paths,true);
        status_.baseline.excluded_layers=Keep(events.layers,true);
        auto* strings=Keep(objects.ManagedType("System","String"));
        if(!status_.baseline.excluded_layers) status_.baseline.excluded_layers=Strings(objects,arrays,strings,{});
        status_.target.model_path=String(objects,target.model_path);
        status_.target.event_paths=Strings(objects,arrays,strings,target.event_files);
        status_.target.excluded_layers=Strings(objects,arrays,strings,target.excluded_layers);
        // Event-only branch. A battle CharacterConfig must not be supplied as
        // AdventureCharacterConfig or used to start gameplay logic.
        status_.operation=access.AllocateOperation(objects); Keep(status_.operation);
        status_.initialize=access.InitializeOperation(status_.operation.object);
        if(!status_.initialize.returned) throw std::runtime_error("official model operation constructor failed");
        status_.prepared=true;
    } catch(const std::exception& error) { status_.errors={error.what()}; }
}
ModelProfileResult ModelProfileInputs::State::Close() {
    status_.prepared=false; ModelProfileResult out;
    for(auto id:handles_) if(id && !roots_.Release(id).complete) out.errors.push_back("model profile input root release unresolved");
    out.complete=out.errors.empty(); status_.closed=out.complete; return out;
}

ModelProfileInputs::ModelProfileInputs(ReferenceMethods refs):state_(new State(refs)) {}
ModelProfileInputs::~ModelProfileInputs() { delete state_; }
const ModelProfileInputStatus& ModelProfileInputs::Status() const { return state_->status_; }
void ModelProfileInputs::Prepare(ManagedObjects& objects,const ManagedArrays& arrays,const ProfileInputs& access,
    ProfileEventFields fields,void* model,void* consumer,const ProfileDescription& target) {
    state_->Prepare(objects,arrays,access,fields,model,consumer,target);
}
ModelProfileResult ModelProfileInputs::Close() { return state_->Close(); }
}
