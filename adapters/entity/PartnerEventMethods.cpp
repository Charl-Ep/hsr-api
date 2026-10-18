#include "../../src/entity/PartnerEventCleanup.hpp"
#include <stdexcept>

namespace hksr::sdk_v1::detail {
PartnerEventMethods ResolvePartnerEventCleanup460(MethodResolver& m,
    const std::vector<DeclaredField>& components,const std::vector<DeclaredField>& world) {
    auto field=[](const auto& rows,const char* owner,const char* name,const char* type) {
        const DeclaredField* found=nullptr;
        for(const auto& row:rows)if(row.owner_namespace=="RPG.GameCore" && row.owner_name==owner && row.name==name) {
            if(found || row.type!=type || !row.flags_observed || (row.flags&0x10) || row.offset<16)
                throw std::invalid_argument(std::string(owner)+"."+name+" declaration differs");
            found=&row;
        }
        if(!found)throw std::invalid_argument(std::string(owner)+"."+name+" declaration absent");
        return found->offset;
    };
    PartnerEventMethods out;
    out.component_owner=field(components,"GameComponentBase","_OwnerRef","RPG.GameCore.GameEntity");
    out.world_events=field(world,"GameWorld","_EventManager","RPG.GameCore.EventManager");
    out.component=m.Resolve("RPG.GameCore","GameEntity","GetComponent","RPG.GameCore.GameComponentBase",{"System.Type"});
    out.owner_world=m.Resolve("RPG.GameCore","GameEntity","get_OwnerWorldRef","RPG.GameCore.GameWorld",{});
    out.delegate_ctor=m.Resolve("RPG.GameCore","OnEventCallBack",".ctor","System.Void",{"System.Object","System.IntPtr"});
    out.callback=m.Resolve("","OLABFALNLJM","HEMJBALNMKB","System.Void",{"EKFMIDBJDIA"});
    out.unregister=m.Resolve("RPG.GameCore","EventManager","UnRegisterEventListener","System.Void",
        {"RPG.GameCore.EventType","System.UInt32","RPG.GameCore.OnEventCallBack"});
    // Qualified 4.6 registration and removal use LevelCharacterDie (66) and
    // the same filter value (1). Neither is an actor's RuntimeID.
    return out;
}
}
