#include <hksr/presentation/effects/EffectRelations.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields,const char* name,const char* type) {
    const DeclaredField* found=nullptr;
    for(const auto& f:fields)if(f.owner_namespace=="RPG.Client" &&
        f.owner_name=="TriggerEffectParams" && f.name==name) {
        if(found || f.type!=type || !f.flags_observed || (f.flags&0x10))
            throw std::invalid_argument(std::string("effect request declaration differs: ")+name);
        found=&f;
    }
    if(!found)throw std::invalid_argument(std::string("effect request field absent: ")+name);
    return found->offset;
}
template<class T>Observed<T> ReadField(void* object,std::size_t offset) {
    Observed<T> result;
    if(object)result.read.returned=native::CopyMemoryChecked(&result.value,
        static_cast<unsigned char*>(object)+offset,sizeof(result.value));
    return result;
}
ObjectResult ReadReference(void* object,std::size_t offset) {
    const auto result=ReadField<void*>(object,offset);
    return {result.read,result.value};
}
}
EffectRelations::EffectRelations(const std::vector<DeclaredField>& fields)
    :caster_(Field(fields,"CasterEntity","RPG.GameCore.GameEntity")),
    target_(Field(fields,"TargetEntity","RPG.GameCore.GameEntity")),
    param_entity_(Field(fields,"ParamEntity","RPG.GameCore.GameEntity")),
    instance_(Field(fields,"<EffectInstance>k__BackingField","RPG.Client.MonoEffect")),
    id_(Field(fields,"<UniqueEffectID>k__BackingField","System.UInt32")),
    released_(Field(fields,"<IsReleased>k__BackingField","System.Boolean")) {}
EffectRelationsSnapshot EffectRelations::Read(EffectReference request) const {
    EffectRelationsSnapshot out;
    out.id_before=ReadField<std::uint32_t>(request.parameters,id_);
    out.caster=ReadReference(request.parameters,caster_);
    out.target=ReadReference(request.parameters,target_);
    out.param_entity=ReadReference(request.parameters,param_entity_);
    out.instance=ReadReference(request.parameters,instance_);
    out.released=ReadField<bool>(request.parameters,released_);
    out.id_after=ReadField<std::uint32_t>(request.parameters,id_);
    out.same_generation=out.id_before.read.returned && out.id_after.read.returned &&
        out.id_before.value==request.id && out.id_after.value==request.id;
    return out;
}
}
