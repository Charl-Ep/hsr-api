#include "EffectViewPolicy.hpp"
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>

namespace hksr::sdk_v1::detail {
namespace {
std::size_t Field(const TimelineRegistryAccess::Fields& fields,const char* owner,const char* name,const char* type) {
    for(const auto& f:fields("RPG.Client",owner))if(f.owner_name==owner && f.name==name && f.type==type &&
        f.flags_observed && !(f.flags&0x10))return f.offset;
    throw std::runtime_error(std::string("effect view field unavailable: ")+owner+"."+name);
}
template<class T>T Read(void* object,std::size_t offset) {
    T value{};
    if(!object || !native::CopyMemoryChecked(&value,static_cast<unsigned char*>(object)+offset,sizeof(value)))
        throw std::runtime_error("effect view field unreadable");
    return value;
}
struct Classifier {
    ObjectMetadata& metadata;TypeRelationships& relationships;ReferenceMethods refs;
    std::size_t ui,list,position;
    std::unique_ptr<ManagedReferenceList> plugins;
    std::vector<std::unique_ptr<ManagedRoots>> unresolved;
    Classifier(ObjectMetadata& m,TypeRelationships& t,ReferenceMethods r,const TimelineRegistryAccess::Fields& fields):metadata(m),relationships(t),refs(r),
        ui(Field(fields,"MonoEffect","<IsUIEffect>k__BackingField","System.Boolean")),
        list(Field(fields,"MonoEffectBehavior","_EffectPluginList","RPG.PoolList<RPG.Client.MonoEffectPluginBase>")),
        position(Field(fields,"MonoEffectPluginFollow","PositionOption","RPG.Client.PositionFollowOption")){}
    bool Classify(void* behavior,void* effect) {
        if(Read<bool>(effect,ui))return true;
        void* collection=Read<void*>(behavior,list);
        // A not-yet-initialized list must not be cached as actor-local.
        if(!collection)throw std::runtime_error("effect plugin list not initialized");
        auto roots=std::make_unique<ManagedRoots>(refs);std::vector<ManagedRootId> held;
        const auto keep=[&](void* p){held.reserve(held.size()+1);held.push_back(roots->Keep(p));if(!roots->Status(held.back()).ready)
            throw std::runtime_error("effect classification root unavailable");};
        const auto release=[&]{
            if(!roots)return true;
            bool complete=true;
            for(auto id:held)if(roots->Release(id).complete)roots->Forget(id);else complete=false;
            if(!complete)unresolved.push_back(std::move(roots));else roots.reset();
            return complete;
        };
        try {
            keep(collection);
            if(!plugins) {
                auto methods=ResolveReferenceListMethods(metadata.ReadObjectType(collection),"RPG.Client.MonoEffectPluginBase");
                if(!methods.binding_errors.empty())throw std::runtime_error(methods.binding_errors.front());
                plugins=std::make_unique<ManagedReferenceList>(std::move(methods));
            }
            const auto count=plugins->Count(collection);
            if(!count.read.returned || count.value<0)throw std::runtime_error("effect plugin count unreadable");
            bool camera=false;
            for(int i=0;i<count.value && !camera;++i) {
                const auto item=plugins->Item(collection,i);
                if(!item.call.returned)throw std::runtime_error("effect plugin unreadable");
                if(!item.object)continue;
                keep(item.object);
                const auto type=metadata.ReadObjectTypeName(item.object);
                if(relationships.TypeDerivesFrom(type.name_space.c_str(),type.name.c_str(),"RPG.Client","MonoEffectPluginFollow"))
                    camera=EffectViewPolicy::CameraSpace(false,Read<int>(item.object,position));
            }
            if(!release())throw std::runtime_error("effect classification GC cleanup unresolved; receipts retained");
            return camera;
        }catch(...){release();throw;}
    }
};
}
EffectViewPolicy::Classify MakeEffectViewClassifier(ObjectMetadata& m,TypeRelationships& t,ReferenceMethods r,TimelineRegistryAccess::Fields fields) {
    const auto state=std::make_shared<Classifier>(m,t,r,fields);
    return [state](void* behavior,void* effect){return state->Classify(behavior,effect);};
}
}
