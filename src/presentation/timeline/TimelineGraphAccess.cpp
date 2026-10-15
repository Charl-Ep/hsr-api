#include "TimelineGraphAccess.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/managed/ManagedRoots.hpp>

namespace hksr::sdk_v1::detail {
namespace {
void CheckField(const std::vector<DeclaredField>& fields,const char* owner,const char* name,const char* type,std::size_t offset) {
    unsigned matches=0;
    for(const auto& f:fields)if(f.owner_namespace=="UnityEngine.Playables" && f.owner_name==owner && f.name==name) {
        ++matches;
        if(f.type!=type || f.offset!=offset || !f.flags_observed || (f.flags&0x10))
            throw std::runtime_error(std::string("Playable value ABI differs: ")+owner+"."+name);
    }
    if(matches!=1)throw std::runtime_error(std::string("Playable value field absent/ambiguous: ")+owner+"."+name);
}
}
struct TimelineGraphAccess::State {
    ObjectMetadata& metadata;ManagedRoots roots;Method graph,valid,resolver;
    std::vector<ManagedRootId> held;
    bool closed=false;
    State(MethodResolver& m,ObjectMetadata& d,ReferenceMethods r,const TimelineRegistryAccess::Fields& fields):metadata(d),roots(r) {
        CheckField(fields("UnityEngine.Playables","Playable"),"Playable","m_Handle","UnityEngine.Playables.PlayableHandle",16);
        for(const auto* name:{"PlayableHandle","PlayableGraph"}) {
            const auto f=fields("UnityEngine.Playables",name);
            CheckField(f,name,"m_Handle","System.IntPtr",16);CheckField(f,name,"m_Version","System.UInt32",24);
        }
        graph=m.Resolve("UnityEngine.Playables","PlayableHandle","GetGraph_Injected","System.Void",
            {"UnityEngine.Playables.PlayableHandle&","UnityEngine.Playables.PlayableGraph&"},MethodKind::Static);
        valid=m.Resolve("UnityEngine.Playables","PlayableGraph","IsValid_Injected","System.Boolean",
            {"UnityEngine.Playables.PlayableGraph&"},MethodKind::Static);
        resolver=m.Resolve("UnityEngine.Playables","PlayableGraph","GetResolver_Injected","UnityEngine.IExposedPropertyTable",
            {"UnityEngine.Playables.PlayableGraph&"},MethodKind::Static);
    }
};
TimelineGraphAccess::TimelineGraphAccess(MethodResolver& m,ObjectMetadata& d,ReferenceMethods r,TimelineRegistryAccess::Fields f)
    :state_(std::make_unique<State>(m,d,r,f)){}
TimelineGraphAccess::~TimelineGraphAccess(){if(!state_->held.empty())state_.release();}
TimelineGraphView TimelineGraphAccess::Read(const void* playable) {
    auto& s=*state_;
    if(s.closed)throw std::logic_error("Timeline graph reader already closed");
    TimelineGraphView out;
    if(!playable || !native::CopyMemoryChecked(&out.playable,playable,sizeof(out.playable)))throw std::runtime_error("Playable callback value unreadable");
    // Copy prevents an injected implementation from modifying the callback's
    // original by-value temporary; padding never participates in identity.
    auto local=out.playable;
    if(!native::CallVoid(s.graph,&local,&out.graph).returned)throw std::runtime_error("Playable graph lookup failed");
    bool valid=false;
    if(!native::Call(s.valid,&valid,&out.graph).returned || !valid)throw std::runtime_error("Playable graph invalid/unreadable");
    if(!native::Call(s.resolver,&out.resolver,&out.graph).returned)throw std::runtime_error("Playable graph resolver lookup failed");
    if(out.resolver) {
        s.held.reserve(s.held.size()+1);const auto id=s.roots.Keep(out.resolver);s.held.push_back(id);
        if(!s.roots.Status(id).ready)throw std::runtime_error("Playable resolver retention failed");
        const auto type=s.metadata.ReadObjectType(out.resolver);
        out.resolver_is_director=type.name_space=="UnityEngine.Playables" && type.name=="PlayableDirector";
    }
    return out;
}
bool TimelineGraphAccess::Close() {
    auto& s=*state_;s.closed=true;
    for(auto i=s.held.begin();i!=s.held.end();) {
        if(s.roots.Release(*i).complete){s.roots.Forget(*i);i=s.held.erase(i);}else ++i;
    }
    return s.held.empty();
}
}
