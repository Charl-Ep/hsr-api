#include <hksr/model/CharacterOutfit.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>
#include <algorithm>

namespace hksr::sdk_v1 {
struct CharacterOutfit::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    ManagedRoots roots;EntityAccess entities;CharacterOutfitStatus status;
    std::vector<ManagedRootId> handles;
    void* component=nullptr;void* original=nullptr;Method replace;
    bool changed=false;
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r,EntityAccessMethods e)
        :methods(m),metadata(d),objects(o),roots(r),entities(std::move(e)){}
    void* Keep(ObjectResult value,bool nullable=false) {
        if(!value.call.returned || (!nullable && !value.object))throw std::runtime_error("outfit object unavailable");
        if(!value.object)return nullptr;
        handles.push_back(0);handles.back()=roots.Keep(value.object);
        if(!roots.Status(handles.back()).ready)throw std::runtime_error("outfit object retention failed");
        return value.object;
    }
    template<class T>T Field(void* object,const char* name,const char* type) {
        unsigned matches=0;std::size_t offset=0;
        for(const auto& f:metadata.ReadObjectFields(object))if(f.name==name && f.type==type && f.flags_observed && !(f.flags&0x10)){++matches;offset=f.offset;}
        T value{};
        if(matches!=1 || !native::CopyMemoryChecked(&value,static_cast<unsigned char*>(object)+offset,sizeof(T)))
            throw std::runtime_error(std::string("outfit field unavailable: ")+name);
        return value;
    }
    template<class... Args>ObjectResult Object(Method method,Args... args) {
        ObjectResult out;out.call=native::Call(method,&out.object,args...);return out;
    }
    struct Model {std::uint32_t id;void* config;};
    std::vector<Model> Models(void* list) {
        ManagedReferenceList access(ResolveReferenceListMethods(metadata.ReadObjectType(list),"FJEOAPBLMKG"));
        const auto count=access.Count(list);
        if(!count.read.returned || count.value<0)throw std::runtime_error("outfit comparison list unread");
        std::vector<Model> out;
        for(int i=0;i<count.value;++i) {
            auto* model=Keep(access.Item(list,i));
            out.push_back({Field<std::uint32_t>(model,"<ID>k__BackingField","System.UInt32"),
                Field<void*>(model,"<Config>k__BackingField","RPG.GameCore.AvatarOutfitModelConfig")});
        }
        return out;
    }
    bool SameConfig(void* a,void* b,ManagedArrays& arrays,Method equal) {
        if(a==b)return true;
        if(!a || !b)return false;
        const auto stringsEqual=[&](void* x,void* y,const char* name) {
            auto* left=Field<void*>(x,name,"System.String");auto* right=Field<void*>(y,name,"System.String");
            bool same=false;
            if(!native::Call(equal,&same,left,right).returned)throw std::runtime_error("outfit string comparison failed");
            return same;
        };
        // Compare declared content, not object addresses: GetModels may create
        // fresh config objects for an unchanged outfit.
        for(bool decorations:{false,true}) {
            const auto* field=decorations?"DecorationConfigList":"PartConfigList";
            const auto* type=decorations?"RPG.GameCore.AvatarOutfitDecorationConfig[]":"RPG.GameCore.AvatarOutfitPartConfig[]";
            auto* left=Field<void*>(a,field,type);auto* right=Field<void*>(b,field,type);
            if(left==right)continue;
            if(!left || !right)return false;
            const auto lc=arrays.Length(left),rc=arrays.Length(right);
            if(!lc.read.returned || !rc.read.returned || lc.value<0 || rc.value<0)throw std::runtime_error("outfit config array unread");
            if(lc.value!=rc.value)return false;
            for(int i=0;i<lc.value;++i) {
                auto* x=Keep(arrays.Get(left,i),true);auto* y=Keep(arrays.Get(right,i),true);
                if(x==y)continue;
                if(!x || !y)return false;
                if(decorations) {if(!stringsEqual(x,y,"PrefabConfigPath"))return false;}
                else if(Field<bool>(x,"IsStatic","System.Boolean")!=Field<bool>(y,"IsStatic","System.Boolean") ||
                    !stringsEqual(x,y,"MaterialKey") || !stringsEqual(x,y,"MeshConfigPath"))return false;
            }
        }
        return true;
    }
    bool MissesConfigChange(void* before,void* after) {
        const auto old=Models(before),next=Models(after);
        if(old.size()!=next.size())return false;
        // Match the official membership test, without assuming list ordering.
        for(const auto& x:old)if(std::none_of(next.begin(),next.end(),[&](const Model& y){return x.id==y.id;}))return false;
        for(const auto& x:next)if(std::none_of(old.begin(),old.end(),[&](const Model& y){return x.id==y.id;}))return false;
        ArrayMethods arrayMethods;
        arrayMethods.length=methods.Resolve("System","Array","get_Length","System.Int32",{});
        arrayMethods.get=methods.Resolve("System","Array","GetValue","System.Object",{"System.Int32"});
        ManagedArrays arrays(arrayMethods);
        const auto equal=methods.Resolve("System","String","op_Equality","System.Boolean",{"System.String","System.String"},MethodKind::Static);
        for(const auto& x:next) {
            bool same=false;
            for(const auto& y:old)if(x.id==y.id && SameConfig(x.config,y.config,arrays,equal)){same=true;break;}
            if(!same)return true;
        }
        return false;
    }
};
CharacterOutfit::CharacterOutfit(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,ReferenceMethods r,EntityAccessMethods e)
    :state_(std::make_unique<State>(m,d,o,r,std::move(e))){}
CharacterOutfit::~CharacterOutfit()=default; // no engine calls
const CharacterOutfitStatus& CharacterOutfit::Status()const{return state_->status;}
void CharacterOutfit::Prepare(GameEntity* entity,std::uint32_t avatar) {
    auto& s=*state_;auto& status=s.status;
    if(status.started || status.closed)throw std::logic_error("outfit override is single-use");
    status.started=true;status.avatar=avatar;
    try {
        if(!entity || !avatar)throw std::invalid_argument("outfit needs explicit owned entity and Avatar ID");
        const auto hero=s.methods.Resolve("RPG.GameCore","CharacterOutfitComponent","IsPlayerAvatarID","System.Boolean",{"System.UInt32"},MethodKind::Static);
        if(!native::Call(hero,&status.applicable,avatar).returned)throw std::runtime_error("official outfit applicability read failed");
        if(!status.applicable)return; // Other characters do not need this assembly layer.
        auto* type=s.Keep(s.objects.ManagedType("RPG.GameCore","CharacterOutfitComponent"));
        s.component=s.Keep(s.entities.Component(entity,type));
        s.original=s.Field<void*>(s.component,"_OverrideOutfitModels","System.Collections.Generic.List<FJEOAPBLMKG>");
        s.Keep({{true,0},s.original},true);
        const auto gender=s.methods.Resolve("RPG.Client","AvatarHelper","GetGender","RPG.GameCore.GenderType",{"System.UInt32"},MethodKind::Static);
        if(!native::Call(gender,&status.gender,avatar).returned)throw std::runtime_error("official Avatar gender read failed");
        const auto row=s.methods.Resolve("RPG.GameCore","AvatarExcelTable","GetData","RPG.GameCore.AvatarRow",{"System.UInt32"},MethodKind::Static);
        auto* source=s.Keep(s.Object(row,avatar));
        status.base_type=s.Field<int>(source,"AvatarBaseType","RPG.GameCore.AvatarBaseType");
        const auto player=s.methods.Resolve("RPG.Client","PlayerOutfitHelper","GetPlayerOutfit","RPG.Client.AvatarOutfit",{},MethodKind::Static);
        auto* outfit=s.Keep(s.Object(player));
        const auto models=s.methods.Resolve("RPG.Client","AvatarOutfit","GetModels","System.Collections.Generic.List<FJEOAPBLMKG>",
            {"RPG.GameCore.GenderType","RPG.GameCore.AvatarBaseType"});
        auto* selected=s.Keep(s.Object(models,outfit,status.gender,status.base_type));
        try {
            const auto sourceModels=s.methods.Resolve("RPG.GameCore","CharacterOutfitComponent","LoadSourceOutfitModels","System.Collections.Generic.List<FJEOAPBLMKG>",{});
            auto* before=s.Keep(s.Object(sourceModels,s.component));
            status.config_refresh_required=s.MissesConfigChange(before,selected);
        }catch(const std::exception& e) {
            // An unread comparison is diagnostic, not a new gate on the
            // already supported official override. Never force an unknown reload.
            status.errors.push_back(std::string("outfit refresh comparison incomplete: ")+e.what());
        }
        s.replace=s.methods.Resolve("RPG.GameCore","CharacterOutfitComponent","OverrideOutfit","System.Void",
            {"System.Collections.Generic.List<FJEOAPBLMKG>","System.Boolean"});
        // The official method may assign before throwing. Retain the original
        // for teardown even when there is no successful return.
        s.changed=true;status.apply=native::CallVoid(s.replace,s.component,selected,false);
        status.applied=status.apply.returned;
        if(!status.applied)throw std::runtime_error("official instance outfit override failed");
        if(status.config_refresh_required) {
            const auto refresh=s.methods.Resolve("RPG.GameCore","CharacterOutfitComponent","RefreshOutfitParts","System.Void",{});
            status.refresh=native::CallVoid(refresh,s.component);
            if(!status.refresh.returned)throw std::runtime_error("official outfit config refresh failed");
        }
    }catch(const std::exception& e){status.errors.push_back(e.what());}
}
bool CharacterOutfit::Close(bool owner_alive) {
    auto& s=*state_;auto& status=s.status;
    if(status.closed)return true;
    if(s.changed && owner_alive && !status.restore_attempted) {
        status.restore_attempted=true;
        status.restore=native::CallVoid(s.replace,s.component,s.original,false);
        if(!status.restore.returned)status.errors.push_back("official outfit restoration failed; not retried");
    }
    // No repeat application/restoration and no global write during teardown.
    for(auto it=s.handles.begin();it!=s.handles.end();) {
        if(!*it || s.roots.Release(*it).complete){if(*it)s.roots.Forget(*it);it=s.handles.erase(it);}else ++it;
    }
    status.closed=s.handles.empty();return status.closed;
}
}
