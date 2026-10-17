#include <hksr/presentation/effects/EffectTriggerSource.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
EffectTriggerSource::EffectTriggerSource(const std::vector<DeclaredField>& fields,ReferenceMethods references,ComponentConfig strings)
    :references_(references),strings_(std::move(strings)) {
    unsigned found=0;
    for(const auto& f:fields)if(f.owner_namespace=="RPG.GameCore" && f.owner_name=="TriggerEffect" && f.name=="EffectPath") {
        if(++found!=1 || f.type!="System.String" || !f.flags_observed || (f.flags&0x10))throw std::invalid_argument("TriggerEffect path declaration differs");
        path_=f.offset;
    }
    if(found!=1)throw std::invalid_argument("TriggerEffect path declaration missing");
}
TextResult EffectTriggerSource::Path(const EffectTriggerExecution& source) const {
    TextResult out;
    if(!source.serial || !source.config){out.error="no qualified TriggerEffect source";return out;}
    ManagedRoots roots(references_);std::vector<ManagedRootId> held;
    const auto keep=[&](void* object){auto id=roots.Keep(object);held.push_back(id);if(!roots.Status(id).ready)throw std::runtime_error("TriggerEffect source retention failed");};
    try {
        keep(source.config);void* path=nullptr;
        if(!native::CopyMemoryChecked(&path,static_cast<unsigned char*>(source.config)+path_,sizeof(path)))throw std::runtime_error("TriggerEffect path unreadable");
        if(path){keep(path);out=strings_.ReadText(path);}else out.read.returned=true;
    }catch(const std::exception& e){out.error=e.what();}
    for(auto i=held.rbegin();i!=held.rend();++i) {
        if(roots.Release(*i).complete)roots.Forget(*i);
        else {out.read.returned=false;out.error="TriggerEffect source GC release incomplete";}
    }
    return out;
}
}
