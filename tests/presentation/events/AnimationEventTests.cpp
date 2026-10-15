#include <hksr/presentation/events/AnimationEvents.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <Windows.h>
#include <stdexcept>
#include <iostream>
namespace {
using namespace hksr::api;
int component,context,caster,list,path,record,info,registered=0,detached=0,disposed=0;
int groups,inline_calls=0;
bool fault=false;
void Check(bool value) { if(!value) throw std::runtime_error("animation event API mismatch"); }
template<class T> Method M(T function) { return {reinterpret_cast<void*>(function),&info}; }
void* __fastcall Context(void* object,void* metadata) { Check(object==&component && metadata==&info); return &context; }
void* __fastcall Caster(void* object,void* metadata) { Check(object==&context && metadata==&info); return &caster; }
void* __fastcall Records(void* object,void* metadata) { Check(object==&component && metadata==&info); return &list; }
void* __fastcall Register(void* object,void* ctx,void* file,void* metadata) {
    Check(object==&component && ctx==&context && file==&path && metadata==&info); ++registered;
    if(fault) RaiseException(0xE0650001,0,0,nullptr); return &record;
}
void __fastcall Detach(void* object,void* item,bool option,void* metadata) {
    Check(object==&component && item==&record && option && metadata==&info); ++detached;
    if(fault) RaiseException(0xE0650002,0,0,nullptr);
}
void __fastcall Dispose(void* object,void* metadata) { Check(object==&record && metadata==&info); ++disposed; }
void* __fastcall RegisterInline(void* object,void* ctx,void* events,void* behaviors,void* input,void* metadata) {
    Check(object==&component && ctx==&context && !events && !behaviors && input==&groups && metadata==&info);
    ++inline_calls;return &record;
}
int __fastcall Count(void* object,void* metadata) { Check(object==&list && metadata==&info); return 1; }
void* __fastcall Item(void* object,int index,void* metadata) {
    Check(object==&list && metadata==&info);
    if(index!=0) RaiseException(0xE0650003,0,0,nullptr);
    return &record;
}
}
void TestAnimationEvents() {
    // Actual declarations: methods.tsv 186,200,214,229,235,2404. Function
    // bodies below are local substitutes, not proof of official effect cleanup.
    DeclaredType events{"","LGBHONLHNPN",{
        {"get_TaskContext","RPG.GameCore.TaskContext",{},false,M(Context)},
        {"get_DynamicAnimEventDataList","System.Collections.Generic.List<LDMKDJJLOIL>",{},false,M(Records)},
        {"HKFMIIENDHD","LDMKDJJLOIL",{"RPG.GameCore.TaskContext","System.String"},false,M(Register)},
        {"HKFMIIENDHD","LDMKDJJLOIL",{"RPG.GameCore.TaskContext","RPG.GameCore.SingleAnimEventConfig[]","RPG.GameCore.SingleAnimBehaviourEvent[]","RPG.GameCore.AnimGroupEventConfig[]"},false,M(RegisterInline)},
        {"NIBMHHELELI","System.Void",{"LDMKDJJLOIL","System.Boolean"},false,M(Detach)}}};
    DeclaredType tasks{"RPG.GameCore","TaskContext",{{"get_CasterEntity","RPG.GameCore.GameEntity",{},false,M(Caster)}}};
    DeclaredType records{"","LDMKDJJLOIL",{{"Dispose","System.Void",{},false,M(Dispose)}}};
    MetadataResolver source({events,tasks,records});
    struct ConsumerField { void* padding[5]; void* value; } field_source{{},&list};
    std::vector<DeclaredField> fields{{"","LGBHONLHNPN","EGKMMHCBAEM","HHECHLOIEJO",offsetof(ConsumerField,value)}};
    const auto methods=ResolveAnimationEventMethods(source,fields); Check(methods.binding_errors.empty());
    AnimationEvents api(methods);
    Check(api.Consumer(&field_source).call.returned && api.Consumer(&field_source).object==&list);
    fields.push_back(fields.back());
    Check(!ResolveAnimationEventMethods(source,fields).has_consumer); fields.pop_back();
    Check(api.Context(&component).object==&context && api.Caster(&context).object==&caster && api.Records(&component).object==&list);
    Check(api.Register(&component,&context,&path).object==&record && registered==1);
    Check(api.RegisterInline(&component,&context,nullptr,nullptr,&groups).object==&record && inline_calls==1);
    Check(!api.RegisterInline(&component,&context,nullptr,nullptr,nullptr).call.returned && inline_calls==1);
    Check(api.Detach(&component,&record,true).returned && detached==1 && disposed==0);
    Check(api.Dispose(&record).returned && disposed==1);
    fault=true;
    Check(api.Register(&component,&context,&path).call.exception==0xE0650001);
    Check(api.Detach(&component,&record,true).exception==0xE0650002 && disposed==1);
    fault=false;
    Check(!api.Register(nullptr,&context,&path).call.returned && registered==2);
    events.methods.pop_back(); MetadataResolver incomplete({events,tasks,records});
    const auto partial=ResolveAnimationEventMethods(incomplete,fields);
    Check(partial.binding_errors.size()==1 && !partial.detach.code && partial.context.code && partial.dispose_record.code);
    Check(AnimationEvents(partial).Context(&component).object==&context);
    // The closed-list contract is a new runtime binding candidate. It is not
    // present in retained game declarations; these methods are synthetic.
    DeclaredType constructed{"System.Collections.Generic","List`1",{
        {"get_Count","System.Int32",{},false,M(Count)},
        {"get_Item","LDMKDJJLOIL",{"System.Int32"},false,M(Item)},
        {"get_Item","System.Object",{"System.Int32"},false,{}}}};
    const auto list_methods=ResolveReferenceListMethods(constructed,"LDMKDJJLOIL");
    Check(list_methods.binding_errors.empty()); ManagedReferenceList entries(list_methods);
    Check(entries.Count(&list).read.returned && entries.Count(&list).value==1 && entries.Item(&list,0).object==&record);
    Check(entries.Item(&list,1).call.exception==0xE0650003 && !entries.Item(nullptr,0).call.returned);
    constructed.methods.erase(constructed.methods.begin()+1);
    const auto no_item=ResolveReferenceListMethods(constructed,"LDMKDJJLOIL");
    Check(no_item.binding_errors.size()==1 && no_item.count.code && !no_item.item.code);
    std::cout<<"Animation event API passed: declared accessors and registrar overload, separate detach/dispose, preserved fault results; no event lifecycle acceptance.\n";
}
