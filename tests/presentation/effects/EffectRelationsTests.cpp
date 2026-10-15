#include <hksr/presentation/effects/EffectRelations.hpp>
#include <cstddef>
#include <stdexcept>

namespace {
using namespace hksr::api;
// Deliberately not the game's offsets. Metadata, not physical 4.6 constants,
// must determine every field. No target/caster identity is inferred by tests.
struct Request {
    int padding[9]{};
    void* target=nullptr;
    std::uint32_t id=0;
    bool released=false;
    void* caster=nullptr;
    void* instance=nullptr;
    void* param_entity=nullptr;
};
void Check(bool condition){if(!condition)throw std::runtime_error("effect relations test failed");}
std::vector<DeclaredField> Fields() {
    auto field=[](const char* name,const char* type,std::size_t offset) {
        return DeclaredField{"RPG.Client","TriggerEffectParams",name,type,offset,6,true};
    };
    return {
        field("TargetEntity","RPG.GameCore.GameEntity",offsetof(Request,target)),
        field("CasterEntity","RPG.GameCore.GameEntity",offsetof(Request,caster)),
        field("ParamEntity","RPG.GameCore.GameEntity",offsetof(Request,param_entity)),
        field("<EffectInstance>k__BackingField","RPG.Client.MonoEffect",offsetof(Request,instance)),
        field("<UniqueEffectID>k__BackingField","System.UInt32",offsetof(Request,id)),
        field("<IsReleased>k__BackingField","System.Boolean",offsetof(Request,released))};
}
}
void TestEffectRelations() {
    EffectRelations reader(Fields());int caster_a,caster_b,target,parameter_owner,effect;
    Request a,b;a.caster=&caster_a;b.caster=&caster_b;a.target=b.target=&target;
    a.param_entity=&parameter_owner;a.instance=&effect;
    const auto first=reader.Read({0,0,&a}),second=reader.Read({0,0,&b});
    Check(first.same_generation && second.same_generation); // ID zero is valid.
    Check(first.caster.object!=second.caster.object && first.target.object==second.target.object);
    Check(first.param_entity.object==&parameter_owner && first.instance.object==&effect);
    Check(first.caster.call.returned && first.target.call.returned && first.released.read.returned);
    a.caster=nullptr;a.instance=nullptr;
    const auto pending=reader.Read({0,0,&a});
    Check(pending.same_generation && pending.caster.call.returned && !pending.caster.object);
    Check(pending.target.object==&target && pending.instance.call.returned && !pending.instance.object);
    a.id=17;a.caster=&caster_b;a.released=true;
    const auto reused=reader.Read({0,0,&a});
    Check(!reused.same_generation && reused.caster.object==&caster_b && reused.released.value);
    Check(reader.Read({17,0,&a}).same_generation); // GC address is not request generation.
    const auto absent=reader.Read({});
    Check(!absent.same_generation && !absent.caster.call.returned && !absent.id_before.read.returned);
    const auto inaccessible=reader.Read({0,0,reinterpret_cast<void*>(1)});
    Check(!inaccessible.same_generation && !inaccessible.target.call.returned);
    auto partial_fields=Fields();partial_fields.front().offset=0x100000000000ull;
    EffectRelations partial(partial_fields);const auto independently_read=partial.Read({17,0,&a});
    Check(independently_read.same_generation && independently_read.caster.call.returned &&
        !independently_read.target.call.returned); // no unrelated-read gate.
    auto rejects=[](std::vector<DeclaredField> fields) {
        try{EffectRelations invalid(fields);return false;}catch(const std::invalid_argument&){return true;}
    };
    auto fields=Fields();fields.pop_back();Check(rejects(fields));
    fields=Fields();fields.push_back(fields.front());Check(rejects(fields));
    fields=Fields();fields.front().type="System.UInt32";Check(rejects(fields));
    fields=Fields();fields.front().flags|=0x10;Check(rejects(fields));
    fields=Fields();fields.front().flags_observed=false;Check(rejects(fields));
}
