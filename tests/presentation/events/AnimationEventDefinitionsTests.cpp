#include <hksr/presentation/events/AnimationEventDefinitions.hpp>
#include <Windows.h>
#include <cstddef>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace hksr::api;
int info,task_list,enter_tasks,exit_tasks,behavior_tasks;
std::map<int,void*> handles;int next=0;
void Check(bool value){if(!value)throw std::runtime_error("event definition assertion failed");}
template<class T>Method M(T f){return {reinterpret_cast<void*>(f),&info};}
int __fastcall Keep(void* p,int,int,void*){handles[++next]=p;return next;}
void* __fastcall Target(int h,void*){return handles.at(h);}
void __fastcall Free(int h,void*){Check(handles.erase(h)==1);}
using Array=std::vector<void*>;
int __fastcall Length(Array* a,void*){return static_cast<int>(a->size());}
void* __fastcall Item(Array* a,int i,void*){return a->at(i);}
int __fastcall TextLength(std::wstring* s,void*){return static_cast<int>(s->size());}
std::uint16_t __fastcall TextChar(std::wstring* s,int i,void*){return s->at(i);}
struct Entry {std::wstring* name;};
struct Event {std::wstring* name;Array* names;void* tasks;};
struct Group {Array* names;void* enter;void* exit;};
struct Behavior {std::wstring* key;void* tasks;};
struct Metadata:ObjectMetadata {
    std::map<void*,std::string> types;
    void* broken=nullptr;
    DeclaredType ReadObjectType(void*)override{throw std::logic_error("no method enumeration");}
    RuntimeTypeName ReadObjectTypeName(void* value)override{return {"RPG.GameCore",types.at(value)};}
    std::vector<DeclaredField> ReadObjectFields(void* object)override {
        const auto type=types.at(object);
        auto f=[&](const char* name,const char* kind,std::size_t offset){return DeclaredField{"RPG.GameCore",type,name,kind,offset,6,true};};
        if(object==broken)return {};
        if(type=="SingleAnimEventConfig")return {
            f("AnimatorStateName","System.String",offsetof(Event,name)),
            f("AnimatorStates","RPG.GameCore.SingleAnimEntry[]",offsetof(Event,names)),
            f("EventList","RPG.GameCore.TaskConfig[]",offsetof(Event,tasks))};
        if(type=="AnimGroupEventConfig")return {
            f("States","RPG.GameCore.AnimGroupEventEntry[]",offsetof(Group,names)),
            f("OnEnter","RPG.GameCore.TaskConfig[]",offsetof(Group,enter)),
            f("OnExit","RPG.GameCore.TaskConfig[]",offsetof(Group,exit))};
        if(type=="SingleAnimBehaviourEvent")return {
            f("EventUniqueKey","System.String",offsetof(Behavior,key)),
            f("EventList","RPG.GameCore.TaskConfig[]",offsetof(Behavior,tasks))};
        return {f("Name","System.String",offsetof(Entry,name))};
    }
};
}
int main() {
    try {
        std::wstring gift=L"Skill02_Gift",finish=L"Skill02",behavior=L"test-event";
        Entry a{&gift},b{&finish};Array names{&a,&b};
        Event first{&gift,nullptr,&task_list},second{nullptr,&names,&task_list};
        Group group{&names,&enter_tasks,&exit_tasks};Behavior special{&behavior,&behavior_tasks};
        Array events{&first,nullptr,&second},groups{&group},behaviors{&special};
        Metadata metadata;metadata.types={{&first,"SingleAnimEventConfig"},{&second,"SingleAnimEventConfig"},
            {&group,"AnimGroupEventConfig"},{&special,"SingleAnimBehaviourEvent"},{&a,"SingleAnimEntry"},{&b,"SingleAnimEntry"}};
        ComponentConfigMethods strings;strings.string_length=M(TextLength);strings.string_character=M(TextChar);
        ComponentConfig text(strings);ManagedArrays arrays({{},M(Length),M(Item),{}, {}});
        ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
        AnimationEventSource source;source.events=&events;source.groups=&groups;source.behaviors=&behaviors;
        auto result=ReadAnimationEventDefinitions(source,metadata,refs,arrays,text);
        Check(result.issues.empty() && result.rows.size()==5 && handles.empty());
        Check(result.rows[0].configurations==&task_list && result.rows[0].states==std::vector<std::string>{"Skill02_Gift"});
        Check(result.rows[1].index==2 && result.rows[1].states==std::vector<std::string>{"Skill02_Gift","Skill02"});
        Check(result.rows[2].part==AnimationEventPart::GroupEnter && result.rows[2].configurations==&enter_tasks);
        Check(result.rows[3].part==AnimationEventPart::GroupExit && result.rows[3].configurations==&exit_tasks);
        Check(result.rows[4].behavior=="test-event" && result.rows[4].configurations==&behavior_tasks);
        metadata.broken=&first;result=ReadAnimationEventDefinitions(source,metadata,refs,arrays,text);
        Check(result.issues.size()==1 && result.issues[0].index==0 && result.rows.size()==4 && handles.empty());
        metadata.broken=nullptr;group.exit=nullptr;result=ReadAnimationEventDefinitions(source,metadata,refs,arrays,text);
        Check(result.rows[3].configurations==nullptr && result.issues.empty());
        result=ReadAnimationEventDefinitions({},metadata,refs,arrays,text);Check(result.rows.empty() && result.issues.empty());
        std::cout<<"event definitions passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
