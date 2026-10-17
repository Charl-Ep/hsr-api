#include <hksr/control/PartnerControlSession.hpp>
#include <stdexcept>

namespace hksr::sdk_v1 {
PartnerControlMethods ResolvePartnerControl460(MethodResolver& m,
    const std::vector<DeclaredField>& components,const std::vector<DeclaredField>& attachment) {
    auto offset=[](const auto& rows,const char* owner,const char* name,const char* type) {
        const DeclaredField* found=nullptr;
        for(const auto& row:rows)if(row.owner_name==owner && row.name==name) {
            if(found || row.type!=type || !row.flags_observed || (row.flags&0x10))
                throw std::invalid_argument(std::string(owner)+"."+name+" declaration differs");
            found=&row;
        }
        if(!found)throw std::invalid_argument(std::string(owner)+"."+name+" declaration absent");
        return found->offset;
    };
    // IL2CPP field offsets include the boxed object's 16-byte header. The
    // qualified x64 value parameter is a 48-byte native segment, not a box.
    struct Field {const char* name;const char* type;std::size_t native_offset;};
    for(const auto& f:std::initializer_list<Field>{
        {"ILDAAHNKMHL","System.String",0}, {"IGKONILBPEO","RPG.GameCore.GameEntity",8},
        {"HFLDHJHJNCF","System.String",16}, {"BDNAGGIPGGN","System.Boolean",24},
        {"FEDLPCHHOEG","System.Boolean",25}, {"KPNGMCPOMHJ","System.Int32",28},
        {"KPAAEDABMDD","RPG.GameCore.CharacterVisiblityFollowFlag",32},
        {"FCKCHPNEEKB","RPG.GameCore.GameEntity",40}})
        if(offset(attachment,"GDBHCNLLPAG",f.name,f.type)!=f.native_offset+16)
            throw std::invalid_argument(std::string("attachment value layout differs: ")+f.name);
    PartnerControlMethods out;
    out.component_owner=offset(components,"GameComponentBase","_OwnerRef","RPG.GameCore.GameEntity");
    out.component=m.Resolve("RPG.GameCore","GameEntity","GetComponent","RPG.GameCore.GameComponentBase",{"System.Type"});
    out.owner_world=m.Resolve("RPG.GameCore","GameEntity","get_OwnerWorldRef","RPG.GameCore.GameWorld",{});
    out.member_name=m.Resolve("","OLABFALNLJM","set_MemberName","System.Void",{"System.String"});
    out.group_leader=m.Resolve("","OLABFALNLJM","get_Leader","RPG.GameCore.GameEntity",{});
    out.attach=m.Resolve("RPG.GameCore","TransformComponent","AttachTo","System.Void",{"GDBHCNLLPAG"});
    out.cancel_smooth_follow=m.Resolve("RPG.GameCore","TransformComponent","CancelSmoothFollow","System.Void",{});
    out.animation_follow=m.Resolve("","EAKGBLEMBLN","GBJGAIBJLBB","System.Void",{"RPG.GameCore.GameEntity"});
    out.visibility_follow=m.Resolve("RPG.GameCore","CharacterVisibleComponent","SetFollowTargetVisibility","System.Void",
        {"RPG.GameCore.GameEntity","RPG.GameCore.CharacterVisiblityFollowFlag"});
    return out;
}
}
