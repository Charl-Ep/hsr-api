#include <hksr/animation/AnimationOverrides.hpp>
#include <hksr/animation/ReadyPresentation.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <Windows.h>
#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace hksr::api;
int entity,model,animator,replacement,types[3],info,parameter,next_root;
auto* owner=reinterpret_cast<GameEntity*>(&entity);
struct Mask {bool active[4]{},enabled[4]{};} mask;
struct Emotion {void* header[2]{};void* mask=&::mask;} emotion;
struct Layer {int current=100,next=0;float time=0.6f,next_time=0;bool transition=false;};
std::array<Layer,4> layers;
std::map<int,void*> roots;
std::vector<std::unique_ptr<std::string>> strings;
std::vector<std::string> presentation_calls;
float layer_weight=1;
bool model_replaced=false,emotion_replaced=false,changing=false,duplicate=false,parameter_missing=false;
bool fault_fade=false,fault_blink=false,fault_speed=false,fault_state=false;
float speed=0.8f;
int fades=0,blink_writes=0,speed_writes=0,queries=0;
std::function<void()> on_state,on_fade,on_speed_get,on_speed_set,on_blink_get,on_blink_set,on_has;
std::function<void()> on_evaluate,on_hash;
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void Once(std::function<void()>& callback){auto fn=std::move(callback);callback={};if(fn)fn();}
template<class F>Method M(F fn){return {reinterpret_cast<void*>(fn),&info};}
void Fault(){RaiseException(0xE0480001,0,0,nullptr);}
int __fastcall Keep(void* value,int,int,void*){roots[++next_root]=value;return next_root;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
void* __fastcall Component(void*,void* type,void*) {
    ++queries;
    if(type==&types[0])return &model;
    if(type==&types[2])return emotion_replaced?static_cast<void*>(&replacement):&emotion;
    return nullptr;
}
void* __fastcall Animator(void*,void*){return model_replaced?&replacement:&animator;}
bool __fastcall Changing(void*,void*){return changing;}
int __fastcall LayerCount(void*,void*){return 4;}
bool __fastcall Has(void*,int layer,int hash,void*) {
    Once(on_has);
    if(hash==102)return layer==1 || layer==3;
    return (hash==5 || hash==6 || hash==101) && (layer==2 || (duplicate && layer==3));
}
int __fastcall Hash(void* name,void*) {
    Once(on_hash);return *static_cast<std::string*>(name)=="DefaultFixture"?101:102;
}
int __fastcall LayerIndex(void*,void* name,void*) {
    const auto& text=*static_cast<std::string*>(name);
    return text=="AddFixture"?1:text=="OverrideFixture"?3:-1;
}
float __fastcall Weight(void*,int,void*){return layer_weight;}
void __fastcall SetWeight(void*,int layer,float value,void*) {
    Check(value==0,"ready reset disables only named layers");presentation_calls.push_back("weight"+std::to_string(layer));
}
void __fastcall Evaluate(void*,float time,void*) {
    Check(time==0,"ready layer flush uses zero time");presentation_calls.push_back("update");Once(on_evaluate);
}
bool __fastcall Transition(void*,int layer,void*){return layers.at(layer).transition;}
void __fastcall StateInfo(void*,int layer,int index,unsigned char* bytes,void*) {
    Once(on_state);if(fault_state)Fault();
    const auto& row=layers.at(layer);const auto hash=index?row.next:row.current;
    const auto time=index?row.next_time:row.time;
    std::memset(bytes,0,36);std::memcpy(bytes,&hash,4);std::memcpy(bytes+8,&hash,4);std::memcpy(bytes+12,&time,4);
}
void __fastcall Fade(void*,int hash,float,int layer,float time,float,void*) {
    ++fades;auto& row=layers.at(layer);row.next=hash;row.next_time=time;row.transition=true;
    presentation_calls.push_back("fade"+std::to_string(layer));
    Once(on_fade);if(fault_fade){fault_fade=false;Fault();}
}
bool __fastcall BlinkActive(void*,int source,void*){return mask.active[source];}
bool __fastcall BlinkValue(void*,int source,void*){Once(on_blink_get);return mask.enabled[source];}
void __fastcall BlinkSet(void*,int source,bool value,void*) {
    ++blink_writes;mask.active[source]=true;mask.enabled[source]=value;
    Once(on_blink_set);if(fault_blink){fault_blink=false;Fault();}
}
int __fastcall ReadyParameter(std::int16_t value,void*){Check(value==2,"official ready parameter enum");return 61;}
int __fastcall ParameterCount(void*,void*){return parameter_missing?0:1;}
void* __fastcall Parameter(void*,int,void*){return &parameter;}
int __fastcall ParameterHash(void*,void*){return 61;}
int __fastcall ParameterType(void*,void*){return 1;}
float __fastcall GetFloat(void*,int,void*){Once(on_speed_get);return speed;}
void __fastcall SetFloat(void*,int,float value,void*) {
    ++speed_writes;speed=value;Once(on_speed_set);if(fault_speed){fault_speed=false;Fault();}
}
struct Objects:ManagedObjects {
    ObjectResult ManagedType(const char*,const char* name)override {
        return {{true,0},std::string(name)=="CharacterModelComponent"?&types[0]:&types[2]};
    }
    ObjectResult NewStringUtf8(const char* text)override {
        strings.push_back(std::make_unique<std::string>(text));return {{true,0},strings.back().get()};
    }
    ObjectResult AllocateObject(const char*,const char*)override{throw std::runtime_error("unexpected object allocation");}
};
ReferenceMethods Refs(){return {M(Keep),M(Target),M(Free),{}, {}};}
EntityAccess Entities(){EntityAccessMethods methods;methods.component=M(Component);return EntityAccess(methods);}
ModelAccess Models(){ModelAccessMethods methods;methods.animator=M(Animator);methods.changing=M(Changing);return ModelAccess(methods);}
AnimationControl Actions() {
    AnimationControlMethods methods;methods.layer_count=M(LayerCount);methods.has_state=M(Has);
    methods.in_transition=M(Transition);methods.state_info=M(StateInfo);methods.cross_fade=M(Fade);
    methods.hash=M(Hash);methods.layer_index=M(LayerIndex);methods.layer_weight=M(Weight);
    methods.set_layer_weight=M(SetWeight);methods.update=M(Evaluate);
    methods.blink={M(BlinkSet),M(BlinkValue),M(BlinkActive),offsetof(Emotion,mask),true};
    methods.ready_parameter_hash=M(ReadyParameter);methods.parameter_count=M(ParameterCount);methods.parameter_at=M(Parameter);
    methods.parameter_hash=M(ParameterHash);methods.parameter_type=M(ParameterType);methods.float_get=M(GetFloat);methods.float_set=M(SetFloat);
    return AnimationControl(methods);
}
struct Fixture {
    EntityToken token=TrackedEntities().Track(owner);
    AnimationResources resources{token,Refs(),Entities()};
    ModelAccess models=Models();
    AnimationControl actions=Actions();
    Objects objects;
    Fixture() {
        model_replaced=emotion_replaced=changing=duplicate=parameter_missing=false;
        fault_fade=fault_blink=fault_speed=fault_state=false;
        on_state=on_fade=on_speed_get=on_speed_set=on_blink_get=on_blink_set=on_has={};
        on_evaluate=on_hash={};strings.clear();presentation_calls.clear();layer_weight=1;
        layers={};mask={};speed=0.8f;fades=blink_writes=speed_writes=queries=0;
        resources.Component(AnimationComponent::Model,objects);
    }
    AnimationOverrideTarget Target(){return {token,resources,models,actions};}
    ~Fixture(){resources.Close();TrackedEntities().Disposed(token);TrackedEntities().Forget(token);}
};
template<class F>void Rejected(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}Check(caught,"expected invalid snapshot rejection");}
}
void TestAnimationOverrides() {
    static_assert(!std::is_copy_constructible_v<ExpressionOverride>);
    static_assert(!std::is_copy_constructible_v<BlinkOverride>);
    static_assert(!std::is_copy_constructible_v<ReadySpeedOverride>);
    {
        Fixture f;
        ExpressionOverride first(f.Target(),&animator,5,0.2f,{});
        Check(first.Apply({}).call.returned && layers[2].next==5,"expression applied on unique secondary layer");
        ExpressionOverride second(f.Target(),&animator,6,0.3f,{},&first);
        Check(first.Release({}).call.returned && layers[2].next==100,"restore baseline during transition");
        Check(second.Apply({}).call.returned && layers[2].next==6,"replacement expression");
        Check(second.Release({}).call.returned && layers[2].next==100 && layers[2].next_time==0.6f,"successive lease retains original baseline/time");
        const auto count=fades;second.Release({});Check(fades==count,"release exactly once");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        layers[2].next=6;const auto count=fades;
        Check(!expression.Release({}).attempted && fades==count,"official expression successor retained");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        Check(expression.ReleaseTo(6,.2f,{}).call.returned && layers[2].next==6 && layers[2].next_time==0,
            "return uses current resolved expression instead of captured baseline");
        const auto count=fades;expression.Release({});Check(fades==count,"return consumes the expression lease");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        layers[2].next=6;const auto count=fades;
        Check(!expression.ReleaseTo(5,.2f,{}).attempted && fades==count,"resolved return preserves a newer expression writer");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        on_has=[]{model_replaced=true;};const auto count=fades;
        Check(!expression.ReleaseTo(6,.2f,{}).attempted && fades==count,"return rechecks model after target query");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        on_has=[]{layers[2].next=6;};const auto count=fades;
        Check(!expression.ReleaseTo(5,.2f,{}).attempted && fades==count,"return retains a successor written during target query");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});
        fault_fade=true;Check(!expression.Apply({}).call.returned,"partial expression failure exposed");
        Check(expression.Release({}).call.returned && layers[2].next==100,"partial expression write recoverable");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        fault_state=true;const auto count=fades;Rejected([&]{expression.Release({});});fault_state=false;
        expression.Release({});Check(fades==count,"unread release never resets blindly or retries");
    }
    {
        Fixture f;duplicate=true;Rejected([&]{ExpressionOverride expression(f.Target(),&animator,5,0,{});});
        Check(fades==0,"ambiguous layer snapshot never writes");duplicate=false;
        bool intent=true;on_has=[&]{intent=false;};
        Rejected([&]{ExpressionOverride expression(f.Target(),&animator,5,0,[&]{return intent;});});
        Check(fades==0,"cancelled snapshot never writes");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});
        model_replaced=true;Check(!expression.Apply({}).attempted && fades==0,"prepared expression not applied to stale model");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        on_state=[]{model_replaced=true;};const auto count=fades;
        Rejected([&]{expression.Release({});});Check(fades==count,"model replacement inside state read blocks subsequent write");
    }
    {
        Fixture f;BlinkOverride blink(f.Target(),3);mask.active[1]=true;mask.enabled[1]=false;
        Check(blink.Apply(f.objects,{}).call.returned && !mask.enabled[3],"requested blink source disabled");
        model_replaced=true;
        Check(blink.Release({}).call.returned && mask.enabled[3] && !mask.enabled[1],"entity blink source restored across Art change; unrelated source retained");
        Check(blink_writes==2,"blink written and restored once");
    }
    {
        Fixture f;mask.active[3]=true;mask.enabled[3]=false;
        BlinkOverride blink(f.Target(),3);Check(!blink.Apply(f.objects,{}).attempted,"preexisting disabled source not owned");
        blink.Release({});Check(blink_writes==0,"preexisting source not changed on release");
    }
    {
        Fixture f;BlinkOverride blink(f.Target(),3);fault_blink=true;
        Check(!blink.Apply(f.objects,{}).call.returned,"partial blink failure exposed");
        Check(blink.Release({}).call.returned && mask.enabled[3],"partial blink cleaned up");
    }
    {
        Fixture f;BlinkOverride blink(f.Target(),3);blink.Apply(f.objects,{});
        emotion_replaced=true;Check(!blink.Release({}).attempted && blink_writes==1,"replacement emotion component not modified");
    }
    {
        Fixture f;BlinkOverride blink(f.Target(),3);on_blink_get=[]{emotion_replaced=true;};
        Check(!blink.Apply(f.objects,{}).attempted && blink_writes==0,"component replacement inside snapshot blocks write");
    }
    {
        Fixture f;BlinkOverride blink(f.Target(),3);blink.Apply(f.objects,{});mask.enabled[3]=true;
        Check(!blink.Release({}).attempted && blink_writes==1,"official blink release retained");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);
        Check(lease.Apply(1,{}).call.returned && lease.Owned(),"ready parameter acquired");
        Check(lease.Apply(-1,{}).call.returned && speed==-1,"same parameter reversed");
        Check(lease.Release({}).call.returned && speed==0.8f,"ready baseline restored not guessed one");
        lease.Release({});Check(speed_writes==3 && !lease.Owned(),"ready release once");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);parameter_missing=true;
        Check(!lease.Apply(1,{}).attempted && !lease.Owned() && speed_writes==0,"missing speed independent of body action");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);fault_speed=true;
        Check(!lease.Apply(1,{}).call.returned && lease.Owned(),"partial speed write tracked");
        Check(lease.Release({}).call.returned && speed==0.8f,"partial speed restored");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);lease.Apply(1,{});speed=0.4f;
        Check(!lease.Release({}).attempted && speed==0.4f,"external parameter change retained");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);bool intent=true;
        on_speed_get=[&]{intent=false;};Check(!lease.Apply(1,[&]{return intent;}).attempted && speed_writes==0,"cancel during speed snapshot prevents write");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);lease.Apply(1,{});
        on_speed_get=[]{model_replaced=true;};Check(!lease.Release({}).attempted && speed_writes==1,"model replacement during release snapshot prevents write");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);lease.Apply(1,{});
        changing=true;Check(lease.Release({}).call.returned && speed==0.8f,"same live Animator still restored while model begins changing");
        ReadySpeedOverride next(f.Target(),&animator);
        Check(!next.Apply(1,{}).attempted,"changing model does not accept new override");
    }
    {
        Fixture f;ReadySpeedOverride lease(f.Target(),&animator);bool rejected=false;
        on_speed_set=[&]{try{lease.Release({});}catch(const std::logic_error&){rejected=true;}};
        Check(lease.Apply(1,{}).call.returned && rejected && lease.Owned(),"synchronous reentrant release cannot invalidate in-flight write");
        lease.Release({});Check(speed==0.8f,"deferred caller can release after unwind");
    }
    {
        Fixture f;ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
        ReadySpeedOverride lease(f.Target(),&animator);lease.Apply(1,{});
        BlinkOverride blink(f.Target(),3);blink.Apply(f.objects,{});
        TrackedEntities().Disposed(f.token);TrackedEntities().Forget(f.token);
        const auto new_token=TrackedEntities().Track(owner);const auto count=queries;
        Check(!expression.Release({}).attempted && !lease.Release({}).attempted && !blink.Release({}).attempted && queries==count,
            "reused address rejected before component access");
        TrackedEntities().Disposed(new_token);TrackedEntities().Forget(new_token);
    }
    {
        Fixture f;
        {ExpressionOverride expression(f.Target(),&animator,5,0,{});expression.Apply({});
         ReadySpeedOverride lease(f.Target(),&animator);lease.Apply(1,{});
         BlinkOverride blink(f.Target(),3);blink.Apply(f.objects,{});}
        Check(fades==1 && speed_writes==1 && blink_writes==1,"destructors never call engine");
    }
    Check(roots.empty(),"override fixture roots closed");
    ReadyAnimationDefaults defaults;
    defaults.default_expression={true,"DefaultFixture",{}};
    defaults.empty_layer_state={true,"EmptyFixture",{}};
    defaults.hit_additive_layer={true,"AddFixture",{}};
    defaults.hit_override_layer={true,"OverrideFixture",{}};
    {
        Fixture f;const auto result=PrepareReadyPresentation(f.Target(),&animator,defaults,f.objects,{});
        Check(result.current && presentation_calls==std::vector<std::string>{"update","fade1","weight1","update","fade3","weight3","fade2"},
            "ready entry official order only named layers; no body action");
    }
    {
        Fixture f;layer_weight=0;
        const auto result=PrepareReadyPresentation(f.Target(),&animator,defaults,f.objects,{});
        Check(result.current && presentation_calls==std::vector<std::string>{"weight1","weight3","fade2"},
            "already inactive hit layers do not evaluate animation events");
    }
    {
        Fixture f;auto optional=defaults;optional.hit_additive_layer.value="Missing";
        layer_weight=0;
        const auto result=PrepareReadyPresentation(f.Target(),&animator,optional,f.objects,{});
        Check(result.current && presentation_calls==std::vector<std::string>{"weight3","fade2"} &&
            result.detail.find("Missing absent")!=std::string::npos,"missing optional hit layer does not block remaining reset");
    }
    {
        Fixture f;on_evaluate=[]{model_replaced=true;};
        Check(!PrepareReadyPresentation(f.Target(),&animator,defaults,f.objects,{}).current &&
            presentation_calls==std::vector<std::string>{"update"},"model replaced by zero-time evaluation stops subsequent old writes");
    }
    {
        Fixture f;bool intent=true;on_evaluate=[&]{intent=false;};
        Check(!PrepareReadyPresentation(f.Target(),&animator,defaults,f.objects,[&]{return intent;}).current &&
            presentation_calls==std::vector<std::string>{"update"},"caller intent change stops remaining ready reset");
    }
    {
        Fixture f;on_hash=[]{model_replaced=true;};
        Check(!PrepareReadyPresentation(f.Target(),&animator,defaults,f.objects,{}).current && presentation_calls.empty(),
            "model change during name hashing stops later layer calls");
    }
    Check(roots.empty(),"ready entry fixture roots closed");
}
