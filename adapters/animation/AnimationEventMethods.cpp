#include <hksr/presentation/events/AnimationEvents.hpp>
#include "../../src/metadata/ResolveBatch.hpp"
namespace hksr::sdk_v1 {
AnimationInputTypes ResolveAnimationInputTypes(ManagedObjects& objects) {
    AnimationInputTypes out;
    out.data=objects.ManagedType("RPG.GameCore","AdventureCharacterDataComponent");
    out.events=objects.ManagedType("","LGBHONLHNPN");
    return out;
}
AnimationEventMethods ResolveAnimationEventMethods(MethodResolver& source,const std::vector<DeclaredField>& fields) {
    detail::ResolveBatch b(source);
    AnimationEventMethods out;
    out.context=b.Resolve("","LGBHONLHNPN","get_TaskContext","RPG.GameCore.TaskContext",{});
    out.caster=b.Resolve("RPG.GameCore","TaskContext","get_CasterEntity","RPG.GameCore.GameEntity",{});
    out.records=b.Resolve("","LGBHONLHNPN","get_DynamicAnimEventDataList","System.Collections.Generic.List<LDMKDJJLOIL>",{});
    out.register_file=b.Resolve("","LGBHONLHNPN","HKFMIIENDHD","LDMKDJJLOIL",{"RPG.GameCore.TaskContext","System.String"});
    out.register_inline=b.Resolve("","LGBHONLHNPN","HKFMIIENDHD","LDMKDJJLOIL",{"RPG.GameCore.TaskContext",
        "RPG.GameCore.SingleAnimEventConfig[]","RPG.GameCore.SingleAnimBehaviourEvent[]","RPG.GameCore.AnimGroupEventConfig[]"});
    out.detach=b.Resolve("","LGBHONLHNPN","NIBMHHELELI","System.Void",{"LDMKDJJLOIL","System.Boolean"});
    out.dispose_record=b.Resolve("","LDMKDJJLOIL","Dispose","System.Void",{});
    out.binding_errors=b.Errors(); // failed mutation binding does not hide getters
    unsigned consumers=0;
    for(const auto& field:fields) if(field.owner_namespace.empty() && field.owner_name=="LGBHONLHNPN" &&
        field.name=="EGKMMHCBAEM" && field.type=="HHECHLOIEJO") {
        ++consumers; out.consumer_offset=field.offset;
    }
    out.has_consumer=consumers==1;
    if(!out.has_consumer) out.binding_errors.push_back("animation consumer identity field missing or ambiguous");
    return out;
}
}
