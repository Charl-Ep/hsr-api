#include <hksr/animation/OfficialCatalog.hpp>
#include <Windows.h>
#include <map>
#include <stdexcept>
#include <memory>
#include <vector>
#include <cstddef>

namespace {
using namespace hksr::api;
void Check(bool value) { if(!value) throw std::runtime_error("official catalog test failed"); }
enum Kind { Text,Table,Values,List,Array,Npc,Player,Avatar,Config,Type,MotionList,Motion,FreeStyleConfig,StatesConfig,StateRow,Number };
struct Node {
    Kind kind=Text; std::wstring text;
    std::vector<void*> items;
    std::uint32_t id=0,avatar=0,enhanced=0,skin=0;
    Node *config=nullptr,*prefab=nullptr,*identity=nullptr,*initial=nullptr,*events=nullptr,*layers=nullptr;
    bool bad=false;
};
std::vector<std::unique_ptr<Node>> storage;
Node *npcs,*players,*avatars,*config,*object_type,*motion_list,*motion,*shared_motion,*freestyle_config,*states_config;
int info,sequence=0,loads=0,copies=0,frees=0;
bool npc_unavailable=false,free_fault=false;
bool state_lists=false,motion_index_unavailable=false;
std::map<int,void*> roots;
Node* New(Kind kind) { storage.push_back(std::make_unique<Node>()); storage.back()->kind=kind; return storage.back().get(); }
Node* String(const wchar_t* text) { auto value=New(Text); value->text=text; return value; }
template<class T>Method M(T f) { return {reinterpret_cast<void*>(f),&info}; }
int __fastcall Retain(void* object,int handle,int kind,void* metadata) { Check(!handle && kind==2 && metadata==&info); roots[++sequence]=object; return sequence; }
void* __fastcall Target(int handle,void*) { return roots.at(handle); }
void __fastcall Free(int handle,void*) { ++frees; if(free_fault) {free_fault=false; RaiseException(0xE0550001,0,0,nullptr);} roots.erase(handle); }
void* __fastcall Npcs(void*) { if(npc_unavailable) RaiseException(0xE0550002,0,0,nullptr); return npcs; }
void* __fastcall Avatars(void*) { return avatars; }
void* __fastcall Players(bool option,void* metadata) { Check(!option && metadata==&info); return players; }
void* __fastcall ValueItems(Node* dictionary,void*) { return dictionary->config; }
int __fastcall Count(Node* value,void*) { return static_cast<int>(value->items.size()); }
void* __fastcall Item(Node* value,int index,void*) { return value->items.at(index); }
void* __fastcall Create(void* type,int count,void*) { Check(type==object_type); auto n=New(Array); n->items.resize(count); return n; }
void __fastcall Copy(Node* values,Node* array,int index,void*) { Check(index==0 && values->items.size()==array->items.size()); ++copies; array->items=values->items; }
int __fastcall Length(Node* value,void*) { return static_cast<int>(value->text.size()); }
std::uint16_t __fastcall Character(Node* value,int index,void*) { return static_cast<std::uint16_t>(value->text.at(index)); }
std::uint32_t __fastcall ID(Node* row,void*) { return row->id; }
std::uint32_t __fastcall AvatarID(Node* row,void*) { return row->avatar; }
std::uint32_t __fastcall Enhanced(Node* row,void*) { return row->enhanced; }
std::uint32_t __fastcall Skin(Node* row,void*) { return row->skin; }
void* __fastcall Path(Node* row,void*) { return row->config; }
void* __fastcall Prefab(Node* row,void*) { return row->prefab; }
void* __fastcall Identity(Node* row,void*) { return row->identity; }
void* __fastcall Load(Node* path,void*) { Check(path && path->kind==Text); ++loads; return config; }
void* __fastcall MotionNames(Node* character,void*) { Check(character->text==L"CharacterA"); if(motion_index_unavailable) RaiseException(0xE0550003,0,0,nullptr); return motion_list; }
std::uint32_t __fastcall MotionID(Node*,Node* name,void*) { return name->text==L"IndexedName"?42u:43u; }
void* __fastcall MotionRow(std::uint32_t id,void*) { return id==42?motion:id==44?shared_motion:nullptr; }
void* __fastcall FreeStyleConfigLoad(Node* path,void*) { Check(path->text==L"FreeStyle/A"); return freestyle_config; }
void* __fastcall StatesConfigLoad(Node* path,void*) { Check(path->text==L"States/A"); return states_config; }
std::uint32_t __fastcall NumberValue(Node* node,void*) { Check(node->kind==Number); return node->id; }
struct Inputs:MethodResolver,ObjectMetadata,ManagedObjects {
    Method Resolve(const char* space,const char* type,const char* name,const char* result,
        std::initializer_list<const char*> args,MethodKind kind) override {
        const std::string t=type,n=name,r=result;
        if(t=="FreeStyleComponent") {
            Check(kind==MethodKind::Instance && std::string(space)=="RPG.GameCore" && r=="System.String" && args.size()==0);
            if(n=="get_FreeStyleCharacterID") return M(Identity);
            Check(n=="get_FreeStyleCharacterConfigPath"); return M(Path);
        }
        Check(kind==MethodKind::Static);
        if(t=="Convert") { Check(std::string(space)=="System" && n=="ToUInt32" && r=="System.UInt32" && args.size()==1 && std::string(*args.begin())=="System.Object"); return M(NumberValue); }
        if(t=="AJKNMKCMKHB") {
            Check(std::string(space).empty());
            if(n=="KCPFICBNOMD") { Check(r=="System.Collections.Generic.List<System.String>" && args.size()==1); return M(MotionNames); }
            if(n=="MCKEFAANJNI") { Check(r=="System.UInt32" && args.size()==2); return M(MotionID); }
            Check(n=="LMBKNCAIDLC" && r=="RPG.GameCore.FreeStyleMotionConfigRow" && args.size()==1); return M(MotionRow);
        }
        Check(std::string(space)=="RPG.GameCore");
        if(t=="NPCDataExcelTable") { Check(n=="get_dataDict" && args.size()==0 && r=="System.Collections.Generic.Dictionary<RPG.GameCore.CommonIndexKey,RPG.GameCore.NPCDataRow>"); return M(Npcs); }
        if(t=="AvatarExcelTable") { Check(n=="get_dataDict" && args.size()==0 && r=="System.Collections.Generic.Dictionary<RPG.GameCore.CommonIndexKey,RPG.GameCore.AvatarRow>"); return M(Avatars); }
        if(t=="AdventurePlayerCommonWrap") { Check(n=="GetAllWrapData" && args.size()==1 && std::string(*args.begin())=="System.Boolean"); return M(Players); }
        Check(t=="GameCoreConfigLoader" && args.size()==1 && std::string(*args.begin())=="System.String");
        if(n=="LoadFreeStyleCharacterConfig") { Check(r=="RPG.GameCore.FreeStyleCharacterConfig"); return M(FreeStyleConfigLoad); }
        if(n=="LoadFreeStyleStateConfig") { Check(r=="RPG.GameCore.FreeStyleStateConfig"); return M(StatesConfigLoad); }
        Check((n=="LoadAdventureCharacterConfig" && r=="RPG.GameCore.AdventureCharacterConfig") ||
            (n=="LoadCharacterConfig" && r=="RPG.GameCore.CharacterConfig")); return M(Load);
    }
    DeclaredType ReadObjectType(void* object) override {
        auto node=static_cast<Node*>(object); DeclaredType type{"Test","Node",{}};
        auto add=[&](const char* n,const char* r,std::vector<std::string> args,Method m) { type.methods.push_back({n,r,std::move(args),false,m}); };
        if(node->kind==Table) add("get_Values","Test.ConstructedValues",{},M(ValueItems));
        else if(node->kind==Values) {
            add("get_Count","System.Int32",{},M(Count));
            add("System.Collections.ICollection.CopyTo","System.Void",{"System.Array","System.Int32"},M(Copy));
        } else if(node->kind==List || node->kind==MotionList) {
            add("get_Count","System.Int32",{},M(Count)); add("get_Item",node->kind==List?"RPG.GameCore.IAdventurePlayerCommonRowWrap":"System.String",{"System.Int32"},M(Item));
        } else if(node->kind==Player) {
            add("get_ID","System.UInt32",{},M(ID)); add("get_AvatarID","System.UInt32",{},M(AvatarID));
            add("get_EnhancedID","System.UInt32",{},M(Enhanced)); add("get_DevSkinID","System.UInt32",{},M(Skin));
            add("get_PlayerJsonPath","System.String",{},M(Path)); add("get_PlayerPrefabPath","System.String",{},M(Prefab));
        } else throw std::runtime_error("unexpected object declaration request");
        return type;
    }
    std::vector<DeclaredField> ReadObjectFields(void* object) override {
        auto node=static_cast<Node*>(object); std::vector<DeclaredField> fields;
        auto add=[&](const char* name,const char* type,std::size_t offset) {fields.push_back({"Test","Node",name,type,offset,0,!node->bad});};
        if(node->kind==Npc) { add("ID","System.UInt32",offsetof(Node,id)); add("JsonPath","System.String",offsetof(Node,config)); add("ConfigEntityPath","System.String",offsetof(Node,prefab)); }
        if(node->kind==Avatar) { add("AvatarID","System.UInt32",offsetof(Node,avatar)); add("JsonPath","System.String",offsetof(Node,config)); add("DefaultAvatarModelPath","System.String",offsetof(Node,prefab)); }
        if(node->kind==Motion) {
            add("ID","System.UInt32",offsetof(Node,id)); add("StartMotion","System.String",offsetof(Node,initial));
            add("FreeStyleCharacterID","System.String",offsetof(Node,identity)); add("StartMotionPath","System.String",offsetof(Node,config));
            // Optional loop path deliberately absent: do not drop a valid ID.
        }
        if(node->kind==FreeStyleConfig) add("FreeStyleStateConfigPath","System.String",offsetof(Node,config));
        if(node->kind==StatesConfig) add("StateList",state_lists?"System.Collections.Generic.List<RPG.GameCore.State>":"RPG.GameCore.State[]",offsetof(Node,events));
        if(node->kind==StateRow) {
            add("MotionID","System.UInt32",offsetof(Node,id)); add("SameAsMotionIDOnLoop","System.UInt32",offsetof(Node,avatar));
            add("EntranceFreeStyleIDList",state_lists?"System.Collections.Generic.List<System.UInt32>":"System.UInt32[]",offsetof(Node,events));
        }
        if(node->kind==Config) {
            add("FreeStyleCharacterID","System.String",offsetof(Node,identity));
            if(!node->bad) add("InitAnimStateName","System.String",offsetof(Node,initial));
            fields.push_back({"Test","Node","AnimEventConfigList","System.String[]",offsetof(Node,events),0,true});
            fields.push_back({"Test","Node","DisableAnimEventLayers","System.String[]",offsetof(Node,layers),0,true});
        }
        return fields;
    }
    ObjectResult NewStringUtf8(const char* value) override { auto n=New(Text); while(*value) n->text.push_back(*value++); return {{true,0},n}; }
    ObjectResult AllocateObject(const char*,const char*) override { throw std::runtime_error("catalog must not allocate entity/config objects"); }
    ObjectResult ManagedType(const char* space,const char* name) override { Check(std::string(space)=="System" && std::string(name)=="Object"); return {{true,0},object_type}; }
};
}
void TestOfficialCatalog() {
    storage.clear(); roots.clear(); sequence=loads=copies=frees=0; npc_unavailable=free_fault=false;
    object_type=New(Type); npcs=New(Table); avatars=New(Table); players=New(List);
    npcs->config=New(Values); avatars->config=New(Values);
    auto npc=New(Npc); npc->id=999; npc->config=String(L"NPC/藿藿.json"); npc->prefab=String(L"NPC/Model.json");
    auto bad=New(Npc); bad->bad=true;
    npcs->config->items={bad,npc};
    auto player=New(Player); player->id=12; player->avatar=777; player->enhanced=1; player->skin=2;
    player->config=String(L"Player/Huohuo.json"); player->prefab=String(L"Player/Prefab.prefab"); players->items={player};
    auto avatar=New(Avatar); avatar->avatar=777; avatar->config=String(L"Battle/Huohuo.json"); avatar->prefab=String(L"Battle/Prefab.prefab");
    avatars->config->items={avatar,avatar}; // provider preserves rows, no arbitrary dedup policy
    config=New(Config); config->identity=String(L"少年_藿藿"); config->initial=String(L"StandBy");
    config->events=New(Array); config->events->items={String(L"Event/A"),String(L"Event/B")}; config->layers=nullptr;
    motion_list=New(MotionList); motion_list->items={String(L"IndexedName"),String(L"MissingRow")};
    motion=New(Motion); motion->id=42; motion->initial=String(L"StartName"); motion->identity=String(L"SharedCharacter"); motion->config=String(L"Animation/Shared");
    shared_motion=New(Motion); *shared_motion=*motion; shared_motion->id=44;
    freestyle_config=New(FreeStyleConfig); freestyle_config->config=String(L"States/A");
    states_config=New(StatesConfig); states_config->events=New(Values);
    auto state=New(StateRow); state->id=42; state->avatar=44; state->events=New(Values);
    auto entrance=New(Number); entrance->id=44; auto missing=New(Number); missing->id=45;
    state->events->items={entrance,missing}; states_config->events->items={state};
    Inputs input; ComponentConfigMethods text; text.string_length=M(Length); text.string_character=M(Character);
    OfficialCatalog catalog(input,input,input,{M(Retain),M(Target),M(Free),{},{}},ManagedArrays({M(Create),M(Count),M(Item),{}, {}}),ComponentConfig(text));
    const auto result=catalog.ReadCharacters();
    Check(result.npcs.enumerated && result.npcs.declared_count==2 && result.npcs.visited_count==2 && result.npcs.rows.size()==1 && result.npcs.issues.size()==1);
    Check(result.npcs.rows[0].character_config==u8"NPC/藿藿.json");
    Check(result.players.enumerated && result.players.rows[0].avatar==777 && result.players.rows[0].skin==2);
    Check(result.avatars.rows.size()==2 && loads==0 && copies==2 && roots.empty());
    auto motions=catalog.ReadNpcMotionIndex("CharacterA");
    Check(motions.motions.enumerated && motions.motions.declared_count==2 && motions.motions.rows.size()==1 && motions.motions.issues.size()==1);
    Check(motions.motions.rows[0].id==42 && motions.motions.rows[0].source_character.value=="SharedCharacter" &&
        !motions.motions.rows[0].loop_path.available && roots.empty() && loads==0);
    auto combined=catalog.ReadNpcMotions("CharacterA","FreeStyle/A");
    Check(combined.state_list_read && combined.configured_ids==std::vector<std::uint32_t>({42,44,45}) && combined.motions.size()==2 && combined.issues.size()==1 && roots.empty());
    Check(combined.motions[1].id==44 && combined.motions[1].indexed_name.empty() && combined.motions[1].source_character.value=="SharedCharacter");
    state_lists=true; auto list_combined=catalog.ReadNpcMotions("CharacterA","FreeStyle/A");
    Check(list_combined.state_list_read && list_combined.motions.size()==2 && list_combined.configured_ids==combined.configured_ids && roots.empty());
    motion_index_unavailable=true; auto only_states=catalog.ReadNpcMotions("CharacterA","FreeStyle/A");
    Check(!only_states.index.motions.enumerated && only_states.state_list_read && only_states.motions.size()==2 && roots.empty());
    motion_index_unavailable=false; state_lists=false;
    auto own_component=New(Config); own_component->identity=String(L"CharacterA"); own_component->config=String(L"FreeStyle/A");
    auto from_component=catalog.ReadNpcMotionsFromComponent(own_component);
    Check(from_component.character_config=="FreeStyle/A" && from_component.index.character=="CharacterA" && from_component.state_list_read && from_component.motions.size()==2 && roots.empty());
    auto identity=catalog.ReadAdventureIdentity("Player/Huohuo.json"); Check(identity.available && identity.value==u8"少年_藿藿" && loads==1 && roots.empty());
    auto profile=catalog.ReadPresentationConfig("Battle/Huohuo.json");
    Check(profile.initial_state.available && profile.initial_state.value=="StandBy" && profile.event_files.values.size()==2 && profile.disabled_event_layers.available && profile.disabled_event_layers.values.empty() && roots.empty());
    config->bad=true; profile=catalog.ReadPresentationConfig("Battle/Huohuo.json");
    Check(!profile.initial_state.available && profile.event_files.available && profile.disabled_event_layers.available); // independent inputs
    npc_unavailable=true; auto partial=catalog.ReadCharacters();
    Check(!partial.npcs.enumerated && partial.players.rows.size()==1 && partial.avatars.rows.size()==2 && roots.empty());
    free_fault=true; catalog.ReadAdventureIdentity("Player/Huohuo.json");
    Check(catalog.UnresolvedReferences()==1 && roots.size()==1);
    const auto prior=frees; catalog.ReadAdventureIdentity("Player/Huohuo.json");
    Check(catalog.UnresolvedReferences()==1 && roots.size()==1 && frees==prior+2); // no retry of unknown free
    roots.clear(); storage.clear(); npc_unavailable=false;
}
