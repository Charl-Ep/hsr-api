#include <hksr/control/LookSupport.hpp>
#include "../serialization/ComponentReferences.hpp"
#include <functional>
#include <set>
#include <optional>
#include <map>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
}
struct LookSupport::State {
    State(ReferenceMethods refs,UnityLifetimeMethods lifetime)
        :roots_(refs),leases_(refs),native_(lifetime,refs),lifetime_(lifetime) {}
    void Prepare(LookSupportServices,void*,const std::string&);
    bool SettleFailure();
    LookSupportResult Close();
    void* Keep(ObjectResult object);
    std::vector<void*> Components(void* root,void* type);
    std::string Text(LookSupportServices services,ObjectResult text);
    struct Node { void* transform=nullptr; int id=0; };
    using Path=std::vector<std::string>;
    using Nodes=std::map<Path,std::vector<Node>>;
    Nodes ReadNodes(LookSupportServices services,void* root,void* transform_type);
    ManagedRoots roots_;
    AssetLeases leases_;
    OwnedUnityObjects native_;
    UnityLifetimeMethods lifetime_;
    std::optional<SceneObjects> scene_;
    std::optional<ManagedArrays> arrays_;
    std::vector<ManagedRootId> handles_;
    std::map<void*,UnityObjectId> created_;
    AssetLeaseId source_lease_=0;
    void* art_=nullptr;
    void* creation_type_=nullptr;
    LookSupportStatus status_;
};
void* LookSupport::State::Keep(ObjectResult value) {
    Require(value.call.returned && value.object,"LookAt input object unavailable");
    handles_.push_back(0); const auto id=roots_.Keep(value.object); handles_.back()=id;
    Require(roots_.Status(id).ready,"LookAt input root unavailable"); return value.object;
}
std::string LookSupport::State::Text(LookSupportServices s,ObjectResult value) {
    auto* text=Keep(value); const auto read=s.config.ReadText(text);
    Require(read.read.returned,"LookAt serialized text unavailable"); return read.utf8;
}
std::vector<void*> LookSupport::State::Components(void* root,void* type) {
    auto* array=Keep(scene_->Components(root,type,true));
    const auto count=arrays_->Length(array);
    Require(count.read.returned && count.value>=0,"LookAt component array unavailable");
    std::vector<void*> out;
    for(int i=0;i<count.value;++i) {
        auto value=arrays_->Get(array,i);
        Require(value.call.returned,"LookAt component read unavailable");
        if(value.object) out.push_back(Keep(value));
    }
    return out;
}
LookSupport::State::Nodes LookSupport::State::ReadNodes(LookSupportServices s,void* root,void* transform_type) {
    auto* anchor=Keep(s.scene.Transform(root));
    struct Entry { Node node; void* parent=nullptr; std::string name; };
    std::map<void*,Entry> entries;
    for(auto* tr:Components(root,transform_type)) {
        const auto id=s.config.InstanceId(tr);
        const auto parent=s.config.Parent(tr);
        Require(id.read.returned && parent.call.returned,"LookAt transform identity unavailable");
        entries.emplace(tr,Entry{{tr,id.value},parent.object,Text(s,s.scene.Name(tr))});
    }
    Require(entries.count(anchor)!=0,"LookAt root absent from transform inventory");
    std::map<void*,Path> paths; paths.emplace(anchor,Path{});
    std::set<void*> visiting;
    std::function<Path(void*)> resolve=[&](void* tr) {
        const auto prior=paths.find(tr); if(prior!=paths.end()) return prior->second;
        Require(entries.count(tr)!=0 && visiting.insert(tr).second,"LookAt transform is outside model or cyclic");
        const auto& entry=entries.at(tr); auto path=resolve(entry.parent); path.push_back(entry.name);
        visiting.erase(tr); paths.emplace(tr,path); return path;
    };
    Nodes out; for(const auto& pair:entries) out[resolve(pair.first)].push_back(pair.second.node);
    return out;
}
void LookSupport::State::Prepare(LookSupportServices s,void* art,const std::string& path) {
    if(status_.started || status_.closed) throw std::logic_error("LookAt support preparation is single-use");
    status_.started=true;
    scene_=s.scene; arrays_=s.arrays;
    try {
        art_=Keep({{true,0},art});
        auto* normal=Keep(s.objects.ManagedType("RPG.Client","LookAtIK"));
        auto* alternate=Keep(s.objects.ManagedType("RPG.Client","ZatzkaLookAtIK"));
        const auto own=Components(art_,normal),own_alternate=Components(art_,alternate);
        if(!own.empty() || !own_alternate.empty()) { status_.existing=true; status_.prepared=true; return; }
        Require(!path.empty(),"official NPC model resource path unavailable");
        auto* source_name=Keep(s.objects.NewStringUtf8(path.c_str()));
        auto* go_type=Keep(s.objects.ManagedType("UnityEngine","GameObject"));
        source_lease_=leases_.Acquire(source_name,go_type,false);
        Require(leases_.Status(source_lease_).ready,"independent NPC LookAt source unavailable");
        auto* source=leases_.Object(source_lease_);
        const auto original=Components(source,normal),source_alternate=Components(source,alternate);
        if(original.empty()) { status_.not_authored=true; return; }
        Require(original.size()==1 && source_alternate.empty(),"NPC LookAt source is ambiguous");
        creation_type_=Keep(s.config.Type(original.front()));
        const auto authored=Text(s,s.config.Serialize(original.front()));
        auto* transform_type=Keep(s.objects.ManagedType("UnityEngine","Transform"));
        const auto source_nodes=ReadNodes(s,source,transform_type),own_nodes=ReadNodes(s,art_,transform_type);
        const auto required=detail::ComponentReferences(authored);
        std::map<int,int> mapping;
        for(const auto& pair:source_nodes) for(const auto& node:pair.second) if(required.count(node.id)) {
            const auto destination=own_nodes.find(pair.first);
            Require(pair.second.size()==1 && destination!=own_nodes.end() && destination->second.size()==1,
                "consumed LookAt transform path is not unique in both models");
            Require(mapping.emplace(node.id,destination->second.front().id).second,"LookAt source instance id is ambiguous");
        }
        const auto remapped=detail::RemapComponentReferences(authored,mapping);
        auto* source_owner=Keep(s.config.Transform(original.front()));
        const Path* owner_path=nullptr;
        for(const auto& pair:source_nodes) for(const auto& node:pair.second) if(node.transform==source_owner) owner_path=&pair.first;
        Require(owner_path!=nullptr,"LookAt owner is outside the source model");
        const auto target=own_nodes.find(*owner_path);
        Require(target!=own_nodes.end() && target->second.size()==1,"LookAt owner correspondence is ambiguous");
        auto* go=Keep(s.visuals.GameObject(target->second.front().transform));
        // Check the actual created type before mutation as well: a derived
        // component cannot silently overlap another private component.
        Require(Components(art_,creation_type_).empty(),"LookAt component already exists on private Art");
        auto* text=Keep(s.objects.NewStringUtf8(remapped.c_str()));
        status_.add_attempted=true;
        status_.addition=s.config.Add(go,creation_type_);
        if(status_.addition.object) created_.emplace(status_.addition.object,native_.Adopt(status_.addition.object));
        Require(status_.addition.call.returned && status_.addition.object,"LookAt component creation outcome unresolved");
        Require(native_.Status(created_.at(status_.addition.object)).root.ready,"created LookAt root unavailable");
        status_.overwrite=s.config.Overwrite(text,status_.addition.object);
        Require(status_.overwrite.returned,"LookAt input write failed");
        const auto actual=detail::ComponentReferences(Text(s,s.config.Serialize(status_.addition.object)));
        std::set<int> expected;
        for(const auto& pair:mapping) expected.insert(pair.second);
        Require(actual==expected,"LookAt readback references differ from private rig");
        status_.remapped_references=mapping.size(); status_.prepared=true;
    } catch(const std::exception& error) {
        status_.errors.push_back(error.what()); status_.rollback_needed=status_.add_attempted;
    }
}
bool LookSupport::State::SettleFailure() {
    if(!status_.rollback_needed || status_.rollback_settled) return true;
    try {
        if(!status_.rollback_observed) {
            const auto alive=UnityObjectAlive(lifetime_.alive,art_);
            Require(alive.read.returned,"private Art native state unavailable during LookAt rollback");
            if(alive.value) for(auto* value:Components(art_,creation_type_))
                if(!created_.count(value)) created_.emplace(value,native_.Adopt(value));
            status_.rollback_observed=true;
        }
        bool dead=true;
        for(const auto& pair:created_) if(!native_.Close(pair.second).native_destroyed) dead=false;
        status_.rollback_settled=dead; return dead;
    } catch(const std::exception& error) { status_.errors={error.what()}; return false; }
}
LookSupportResult LookSupport::State::Close() {
    LookSupportResult out;
    if(status_.closed) { out.complete=true; return out; }
    if(!SettleFailure()) out.errors.push_back("LookAt partial component rollback unresolved");
    for(const auto& pair:created_) {
        // Successful component belongs to the private Art/official lifecycle.
        // Never remove it merely because this C++ wrapper is closing.
        const auto state=status_.rollback_needed?native_.Close(pair.second):native_.ObserveEnd(pair.second);
        if(!state.complete) out.errors.push_back("LookAt component lifetime unresolved");
    }
    if(!out.errors.empty()) return out;
    if(source_lease_ && !leases_.Release(source_lease_).complete) out.errors.push_back("LookAt source lease release unresolved");
    for(auto id:handles_) if(id && !roots_.Release(id).complete) out.errors.push_back("LookAt root release unresolved");
    status_.closed=out.errors.empty(); out.complete=status_.closed; return out;
}

LookSupport::LookSupport(ReferenceMethods refs,UnityLifetimeMethods lifetime):state_(new State(refs,lifetime)) {}
LookSupport::~LookSupport() {delete state_;}
void LookSupport::Prepare(LookSupportServices services,void* art,const std::string& path) {state_->Prepare(services,art,path);}
bool LookSupport::SettleFailure() {return state_->SettleFailure();}
bool LookSupport::CanSubmit() const {const auto& s=state_->status_;return s.started && (!s.rollback_needed || s.rollback_settled);}
LookSupportResult LookSupport::Close() {return state_->Close();}
const LookSupportStatus& LookSupport::Status() const {return state_->status_;}
}
