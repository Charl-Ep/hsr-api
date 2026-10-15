#include <hksr/presentation/events/AnimationExitConfig.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <map>
#include <set>
#include <stdexcept>

namespace hksr::sdk_v1 {
struct AnimationExitConfig::State {
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;TypeRelationships& relationships;
    ManagedRoots roots;ManagedArrays arrays;std::vector<ManagedRootId> held;
    struct Field{void* object;Method get,set;};std::map<std::string,Field> fields;
    std::map<std::string,void*> abilities;std::set<void*> retained;
    std::vector<std::string> errors;std::string operation="retain inputs";void* groups=nullptr;bool attempted=false,closed=false;
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,TypeRelationships& t,ReferenceMethods r,ArrayMethods a)
        :methods(m),metadata(d),objects(o),relationships(t),roots(r),arrays(std::move(a)){}
    bool ConfigReference(const std::string& name) {
        const auto dot=name.rfind('.');
        return dot!=std::string::npos && name.find_first_of("<[]>&*")==std::string::npos &&
            relationships.TypeDerivesFrom(name.substr(0,dot).c_str(),name.substr(dot+1).c_str(),"RPG.GameCore","JsonConfig");
    }
    static void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
    void* Keep(ObjectResult r,bool nullable=false) {
        if(!r.call.returned)throw std::runtime_error("managed call failed; exception="+std::to_string(r.call.exception));
        Require(r.object || nullable,"animation exit input unavailable");
        if(!r.object || retained.count(r.object))return r.object;
        held.push_back(0);held.back()=roots.Keep(r.object);
        Require(roots.Status(held.back()).ready,"animation exit retention failed");retained.insert(r.object);return r.object;
    }
    template<class... A>void* Object(Method m,A... a){ObjectResult r;r.call=native::Call(m,&r.object,a...);return Keep(r,true);}
    template<class... A>void Call(Method m,A... a){const auto r=native::CallVoid(m,a...);if(!r.returned)throw std::runtime_error("managed call failed; exception="+std::to_string(r.exception));}
    void* Text(const std::string& text){return Keep(objects.NewStringUtf8(text.c_str()));}
    Field& Resolve(const std::string& owner,const std::string& name) {
        const auto key=owner+"."+name;const auto found=fields.find(key);if(found!=fields.end())return found->second;
        const auto dot=owner.rfind('.');Require(dot!=std::string::npos,"configuration owner must be qualified");
        auto* type=Keep(objects.ManagedType(owner.substr(0,dot).c_str(),owner.substr(dot+1).c_str()));
        auto* field=Object(methods.Resolve("System","RuntimeType","GetField","System.Reflection.FieldInfo",
            {"System.String","System.Reflection.BindingFlags"}),type,Text(name),54);
        Require(field!=nullptr,"animation exit configuration field absent");
        const auto actual=metadata.ReadObjectType(field);MetadataResolver resolver({actual});
        return fields.emplace(key,Field{field,
            resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"GetValue","System.Object",{"System.Object"}),
            resolver.Resolve(actual.name_space.c_str(),actual.name.c_str(),"SetValue","System.Void",{
                "System.Object","System.Object","System.Reflection.BindingFlags","System.Reflection.Binder","System.Globalization.CultureInfo"})}).first->second;
    }
    void* Get(const std::string& owner,const std::string& name,void* object){operation="read "+owner+"."+name;auto& f=Resolve(owner,name);return Object(f.get,f.object,object);}
    void Set(const char* owner,const char* name,void* object,void* value){operation=std::string("set private ")+owner+"."+name;auto& f=Resolve(owner,name);Call(f.set,f.object,object,value,0,static_cast<void*>(nullptr),static_cast<void*>(nullptr));}
    void* Scalar(const char* type,const char* value) {
        auto* target=Keep(objects.ManagedType("System",type));
        return Object(methods.Resolve("System","Convert","ChangeType","System.Object",{"System.Object","System.Type"},MethodKind::Static),Text(value),target);
    }
    void* New(const char* type) {
        operation=std::string("construct ")+type;
        Require(!relationships.TypeDerivesFrom("RPG.GameCore",type,"System","ValueType"),"animation exit constructor requires a reference type");
        auto* value=Keep(objects.AllocateObject("RPG.GameCore",type));
        Call(methods.Resolve("RPG.GameCore",type,".ctor","System.Void",{}),value);return value;
    }
    void* Array(const char* type,const std::vector<void*>& values) {
        operation=std::string("build array ")+type+" count="+std::to_string(values.size());
        auto* element=Keep(objects.ManagedType("RPG.GameCore",type));
        auto* array=Keep(arrays.Create(element,static_cast<int>(values.size())));
        for(std::size_t i=0;i<values.size();++i)Require(arrays.Set(array,values[i],static_cast<int>(i)).returned,"animation exit array assignment failed");
        return array;
    }
    void* Source(const EffectRemovalSource& source) {
        auto found=abilities.find(source.ability);
        if(found==abilities.end()) {
            operation="load ability "+source.ability;
            auto* ability=Object(methods.Resolve("RPG.GameCore","AbilityJsonTable","GetTurnBasedAbility",
                "RPG.GameCore.TurnBasedAbilityConfig",{"System.String"},MethodKind::Static),Text(source.ability));
            Require(ability!=nullptr,"animation exit ability absent");found=abilities.emplace(source.ability,ability).first;
        }
        void* value=found->second;
        for(const auto& step:source.path) {
            Require(value!=nullptr,"animation exit source path is null");
            value=Get(step.declaring_type,step.field,value);
            operation="select "+step.declaring_type+"."+step.field+" index="+std::to_string(step.index)+" key="+step.key;
            if(step.selection==ConfigSelection::Index) {
                Require(step.index>=0 && value,"animation exit source index absent");
                const auto type=metadata.ReadObjectTypeName(value);
                if(type.name.size()>2 && type.name.compare(type.name.size()-2,2,"[]")==0)value=Keep(arrays.Get(value,step.index),true);
                else {
                    const auto list=metadata.ReadObjectType(value);Method item{};unsigned matches=0;
                    for(const auto& m:list.methods)if(m.name=="get_Item" && !m.is_static && m.parameters==std::vector<std::string>{"System.Int32"} && ConfigReference(m.result)){item=m.entry;++matches;}
                    Require(matches==1,"animation exit source list is ambiguous");value=Object(item,value,step.index);
                }
            } else if(step.selection==ConfigSelection::StringKey) {
                Require(value!=nullptr,"animation exit dictionary absent");
                const auto dict=metadata.ReadObjectType(value);Method item{};unsigned matches=0;
                for(const auto& m:dict.methods)if(m.name=="get_Item" && !m.is_static && m.parameters==std::vector<std::string>{"System.String"} && ConfigReference(m.result)){item=m.entry;++matches;}
                Require(matches==1,"animation exit source dictionary is ambiguous");value=Object(item,value,Text(step.key));
            }
        }
        Require(value!=nullptr,"animation exit task absent");
        const auto task=metadata.ReadObjectTypeName(value);
        Require(task.name_space=="RPG.GameCore" && task.name=="RemoveEffect","animation exit source is not RemoveEffect");
        auto* target=Get("RPG.GameCore.RemoveEffect","TargetType",value);Require(target!=nullptr,"animation exit target absent");
        const auto target_type=metadata.ReadObjectTypeName(target);
        Require(target_type.name_space=="RPG.GameCore" && target_type.name=="TargetAlias","animation exit target is not an alias");
        auto* alias=Get("RPG.GameCore.TargetAlias","Alias",target);bool equal=false;
        Require(native::Call(methods.Resolve("System","String","Equals","System.Boolean",{"System.String","System.String"},MethodKind::Static),
            &equal,alias,Text("Caster")).returned && equal,"animation exit target is not this actor");
        // Keep the complete original data (flags, fade policy, nested fields).
        // A private task clone prevents a consumer from writing shared config.
        operation="clone RemoveEffect";
        auto* copy=Object(methods.Resolve("System","Object","MemberwiseClone","System.Object",{}),value);
        Require(copy && copy!=value,"animation exit task clone is not private");return copy;
    }
};
AnimationExitConfig::AnimationExitConfig(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,TypeRelationships& t,ReferenceMethods r,ArrayMethods a)
    :state_(new State(m,d,o,t,r,std::move(a))){}
AnimationExitConfig::~AnimationExitConfig(){delete state_;}
void* AnimationExitConfig::Groups()const{return state_->groups;}
const std::vector<std::string>& AnimationExitConfig::Errors()const{return state_->errors;}
bool AnimationExitConfig::Prepare(const std::string& name,const std::vector<EffectRemovalSource>& sources) {
    auto& s=*state_;if(s.attempted || s.closed)return false;s.attempted=true;
    try {
        State::Require(!name.empty(),"animation exit state is empty");std::vector<void*> tasks;
        for(const auto& source:sources)try{tasks.push_back(s.Source(source));}
            catch(const std::exception& e){s.errors.push_back(source.ability+": "+s.operation+": "+e.what());}
        State::Require(!tasks.empty(),"no independently readable animation exit task");
        auto* entry=s.New("AnimGroupEventEntry");
        s.Set("RPG.GameCore.AnimGroupEventEntry","Name",entry,s.Text(name));
        s.Set("RPG.GameCore.AnimGroupEventEntry","NormalizedTime",entry,s.Scalar("Single","0"));
        s.Set("RPG.GameCore.AnimGroupEventEntry","ExitNormalizedTime",entry,s.Scalar("Single","-1"));
        s.Set("RPG.GameCore.AnimGroupEventEntry","EnterInTransition",entry,s.Scalar("Boolean","True"));
        s.Set("RPG.GameCore.AnimGroupEventEntry","ExitInTransition",entry,s.Scalar("Boolean","True"));
        s.Set("RPG.GameCore.AnimGroupEventEntry","DisallowExitTriggeredOnStateUpdate",entry,s.Scalar("Boolean","True"));
        auto* group=s.New("AnimGroupEventConfig");
        s.Set("RPG.GameCore.AnimGroupEventConfig","States",group,s.Array("AnimGroupEventEntry",{entry}));
        s.Set("RPG.GameCore.AnimGroupEventConfig","OnEnter",group,s.Array("TaskConfig",{}));
        s.Set("RPG.GameCore.AnimGroupEventConfig","OnExit",group,s.Array("TaskConfig",tasks));
        s.Set("RPG.GameCore.AnimGroupEventConfig","ForceCheckEnteredWhenTriggerExit",group,s.Scalar("Boolean","True"));
        s.groups=s.Array("AnimGroupEventConfig",{group});return true;
    }catch(const std::exception& e){s.errors.push_back(s.operation+": "+e.what());return false;}
}
bool AnimationExitConfig::Close() {
    auto& s=*state_;
    for(auto it=s.held.begin();it!=s.held.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.held.erase(it);}else ++it;
    }
    s.closed=s.held.empty();return s.closed;
}
}
