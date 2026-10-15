#include "TimelineRegistryAccess.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <set>

namespace hksr::sdk_v1::detail {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields,const char* owner,const char* name,const char* type) {
    const DeclaredField* found=nullptr;
    for(const auto& f:fields)if(f.owner_namespace.empty() && f.owner_name==owner && f.name==name) {
        if(found || f.type!=type || !f.flags_observed || (f.flags&0x10))
            throw std::runtime_error(std::string("Timeline field declaration differs: ")+owner+"."+name);
        found=&f;
    }
    if(!found)throw std::runtime_error(std::string("Timeline field absent: ")+owner+"."+name);
    return found->offset;
}
template<class T>T Value(void* object,std::size_t offset) {
    T value{};
    if(!object || !native::CopyMemoryChecked(&value,static_cast<unsigned char*>(object)+offset,sizeof(value)))
        throw std::runtime_error("Timeline declared field unreadable");
    return value;
}
TimelinePhase Phase(std::int32_t value) {
    // Qualified 4.6 DirectorState, not a cast assuming future enum stability.
    switch(value) {
    case 0:return TimelinePhase::Available; case 1:return TimelinePhase::Preparing;
    case 2:return TimelinePhase::Playing; case 3:return TimelinePhase::Paused; case 4:return TimelinePhase::Stopped;
    default:throw std::runtime_error("unrecognized Timeline DirectorState: "+std::to_string(value));
    }
}
}
struct TimelineRegistryAccess::State {
    ObjectMetadata& metadata;ManagedRoots roots;ComponentConfig strings;
    std::vector<ManagedRootId> held;std::set<void*> retained;
    std::size_t list,dictionary,director,resource,name,id,phase;
    bool closed=false;
    State(MethodResolver& methods,ObjectMetadata& d,ReferenceMethods refs,const Fields& fields)
        :metadata(d),roots(refs),strings(ResolveComponentConfigMethods(methods)) {
        const auto registry=fields("","HPPNAAGGAMP"),record=fields("","EBMMGODFEEO");
        list=Field(registry,"HPPNAAGGAMP","MKDJOLKJJON","System.Collections.Generic.List<EBMMGODFEEO>");
        dictionary=Field(registry,"HPPNAAGGAMP","PIJENJPJFLL","System.Collections.Generic.Dictionary<UnityEngine.Playables.PlayableDirector,EBMMGODFEEO>");
        director=Field(record,"EBMMGODFEEO","LDALFGDPLLF","UnityEngine.Playables.PlayableDirector");
        resource=Field(record,"EBMMGODFEEO","DKMKBPLKNDC","System.String");
        name=Field(record,"EBMMGODFEEO","FEBCMBILJMP","System.String");
        id=Field(record,"EBMMGODFEEO","PHFMCACHFIJ","System.Int32");
        phase=Field(record,"EBMMGODFEEO","MEPFOEEGBEA","EBMMGODFEEO.DirectorState");
    }
    void Keep(void* object) {
        if(closed)throw std::logic_error("Timeline registry reader already closed");
        if(!object || retained.count(object))return;
        held.reserve(held.size()+1);const auto handle=roots.Keep(object);held.push_back(handle);
        if(!roots.Status(handle).ready)throw std::runtime_error("Timeline registry GC retention failed");
        retained.insert(object);
    }
    void Check(void* object,const char* type) {
        if(!object)throw std::runtime_error(std::string("null Timeline ")+type);
        Keep(object);const auto actual=metadata.ReadObjectType(object);
        if(!actual.name_space.empty() || actual.name!=type)
            throw std::runtime_error(std::string("Timeline object type differs: expected ")+type);
    }
    void* Ref(void* object,std::size_t offset) {auto* value=Value<void*>(object,offset);Keep(value);return value;}
};
TimelineRegistryAccess::TimelineRegistryAccess(MethodResolver& m,ObjectMetadata& d,ReferenceMethods refs,Fields fields)
    :state_(std::make_unique<State>(m,d,refs,fields)) {}
TimelineRegistryAccess::~TimelineRegistryAccess() {
    // No engine calls during destruction. Preserve uncertain GC receipts rather
    // than retry a decrement or hide a failed cleanup. Normal users call Close.
    if(!state_->held.empty())state_.release();
}
void TimelineRegistryAccess::Hold(void* object){state_->Keep(object);}
void* TimelineRegistryAccess::Find(void* registry,void* director) {
    auto& s=*state_;s.Check(registry,"HPPNAAGGAMP");s.Keep(director);
    if(!director)return nullptr;
    auto* dictionary=s.Ref(registry,s.dictionary);
    if(!dictionary)throw std::runtime_error("Timeline Director dictionary absent");
    const auto type=s.metadata.ReadObjectType(dictionary);MetadataResolver methods({type});
    // The constructed object's own declaration supplies its MethodInfo, including
    // generic context. Do not resolve the open Dictionary`2 or call its code bare.
    const auto find=methods.Resolve(type.name_space.c_str(),type.name.c_str(),"TryGetValue","System.Boolean",
        {"UnityEngine.Playables.PlayableDirector","EBMMGODFEEO&"});
    bool found=false;void* record=nullptr;
    if(!native::Call(find,&found,dictionary,director,&record).returned)
        throw std::runtime_error("Timeline Director dictionary lookup failed");
    if(!found)return nullptr;
    s.Check(record,"EBMMGODFEEO");
    if(s.Ref(record,s.director)!=director)throw std::runtime_error("Timeline Director dictionary/record disagree");
    return record;
}
TimelineRegistryRecord TimelineRegistryAccess::Read(void* registry,void* record) {
    auto& s=*state_;s.Check(registry,"HPPNAAGGAMP");s.Check(record,"EBMMGODFEEO");
    TimelineRegistryRecord out;out.object=record;
    out.view.key={reinterpret_cast<std::uintptr_t>(registry),reinterpret_cast<std::uintptr_t>(record)};
    out.view.playback=Value<std::int32_t>(record,s.id);out.view.phase=Phase(Value<std::int32_t>(record,s.phase));
    out.director=s.Ref(record,s.director);out.resource=s.Ref(record,s.resource);out.runtime_name=s.Ref(record,s.name);
    out.resource_is_null=!out.resource;
    if(out.resource) {
        const auto text=s.strings.ReadText(out.resource);
        if(!text.read.returned)throw std::runtime_error("Timeline resource text unreadable: "+text.error);
        out.view.resource=text.utf8;
    }
    return out;
}
TimelineRegistryRows TimelineRegistryAccess::Rows(void* registry) {
    auto& s=*state_;TimelineRegistryRows out;
    try {
        s.Check(registry,"HPPNAAGGAMP");auto* list=s.Ref(registry,s.list);
        if(!list)throw std::runtime_error("Timeline record list absent");
        ManagedReferenceList access(ResolveReferenceListMethods(s.metadata.ReadObjectType(list),"EBMMGODFEEO"));
        const auto count=access.Count(list);
        if(!count.read.returned || count.value<0)throw std::runtime_error("Timeline record count unreadable");
        for(std::int32_t i=0;i<count.value;++i) {
            try {
                const auto item=access.Item(list,i);
                if(!item.call.returned)throw std::runtime_error("Timeline record list item unreadable");
                out.records.push_back(Read(registry,item.object));
            }catch(const std::exception& e){out.complete=false;out.errors.push_back("row "+std::to_string(i)+": "+e.what());}
        }
    }catch(const std::exception& e){out.complete=false;out.errors.push_back(e.what());}
    return out;
}
bool TimelineRegistryAccess::Close() {
    auto& s=*state_;s.closed=true;
    for(auto i=s.held.begin();i!=s.held.end();) {
        if(s.roots.Release(*i).complete){s.roots.Forget(*i);i=s.held.erase(i);}else ++i;
    }
    return s.held.empty();
}
}
