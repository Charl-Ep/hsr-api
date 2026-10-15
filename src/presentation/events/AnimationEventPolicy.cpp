#include <hksr/presentation/events/AnimationEventPolicy.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include "AnimationConfigSchema.hpp"
#include "../../platform/NativeHooks.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <sstream>

namespace hksr::sdk_v1 {
struct AnimationEventPolicy::State {
    struct Arguments {void *receiver,*events,*behaviors,*groups,*logic,*context,*registration,*file,*metadata;};
    using Original=void(__fastcall*)(void*,void*,void*,void*,void*,void*,void*,void*,void*);
    struct Router {
        detail::NativeHooks hook;
        std::mutex install;
        std::shared_ptr<State> current;
        // Retain failed root-cleanup receipts until process end. Never destroy
        // them and pretend their GC handles were released.
        std::vector<std::shared_ptr<State>> unresolved;
    };
    static Router& Route() {static auto* router=new Router;return *router;}
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    ReferenceMethods references;ArrayMethods arrays;Method caster;
    ComponentConfig strings;
    std::vector<SelfPerformerRule> self_performers;
    detail::AnimationConfigSchema schema;
    Owns owns;Trace trace;
    std::atomic<DWORD> thread{0};
    std::vector<std::unique_ptr<detail::ManagedConfigProjection>> pending;
    struct Invocation {
        std::shared_ptr<State> state;
        std::unique_ptr<detail::ManagedConfigProjection> projection;
        detail::ConfigProjection result;
        GameEntity* entity=nullptr;
        std::string file;
        ULONGLONG started=0;
    };
    State(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,TypeRelationships& relations,Fields f,
        ReferenceMethods r,Owns owner,Trace log,std::vector<std::string> disabled,std::vector<SelfPerformerRule> rules)
        :methods(m),metadata(d),objects(o),references(r),arrays(ResolveArrayMethods(m)),
        caster(m.Resolve("RPG.GameCore","TaskContext","get_CasterEntity","RPG.GameCore.GameEntity",{})),
        strings(ResolveComponentConfigMethods(m)),
        self_performers(std::move(rules)),
        schema(relations,std::move(f),std::set<std::string>(disabled.begin(),disabled.end()),
            [this](void* task,const auto& type,const auto& fields){return NonSelfPerformer(task,type,fields);}),owns(std::move(owner)),trace(std::move(log)) {}
    bool NonSelfPerformer(void* task,const DeclaredType& type,const std::vector<DeclaredField>& fields) {
        const auto name=type.name_space+"."+type.name;
        for(const auto& rule:self_performers)if(rule.task_type==name) {
            std::string alias,reason;
            try {
                const DeclaredField* field=nullptr;
                for(const auto& value:fields)if(value.name==rule.performer_field && value.type=="RPG.GameCore.TargetEvaluator" &&
                    value.flags_observed && !(value.flags&0x10)) {
                    if(field)throw std::runtime_error("ambiguous performer field");field=&value;
                }
                void* target=nullptr;
                if(!task || !field || !native::CopyMemoryChecked(&target,static_cast<unsigned char*>(task)+field->offset,sizeof(target)) || !target)
                    throw std::runtime_error("performer missing or unreadable");
                const auto target_type=metadata.ReadObjectTypeName(target);
                if(target_type.name_space!="RPG.GameCore" || target_type.name!="TargetAlias")throw std::runtime_error("performer is not a literal alias");
                const auto target_fields=metadata.ReadObjectFields(target);field=nullptr;
                for(const auto& value:target_fields)if(value.name=="Alias" && value.type=="System.String" && value.flags_observed && !(value.flags&0x10)) {
                    if(field)throw std::runtime_error("ambiguous alias field");field=&value;
                }
                void* text=nullptr;
                if(!field || !native::CopyMemoryChecked(&text,static_cast<unsigned char*>(target)+field->offset,sizeof(text)))throw std::runtime_error("performer alias unreadable");
                if(text){const auto value=strings.ReadText(text);if(!value.read.returned)throw std::runtime_error("performer alias text unreadable");alias=value.utf8;}
                if(alias=="Caster")return false;
                reason="performer="+alias;
            }catch(const std::exception& error){reason=error.what();}
            Log("disabled external-performer task="+name+" field="+rule.performer_field+" "+reason);
            return true;
        }
        return false;
    }
    void Log(const std::string& text) noexcept {try{if(trace)trace("event configuration policy: "+text);}catch(...) {}}
    void Close(std::unique_ptr<detail::ManagedConfigProjection>& projection) noexcept {
        if(!projection)return;
        try {
            if(projection->Close()){projection.reset();return;}
            Log("GC cleanup incomplete; receipt retained");pending.push_back(std::move(projection));
        }catch(...) {
            // Keep an existing owner even if bookkeeping allocation failed.
            // Emergency retention leaks only on OOM/cleanup failure, never
            // releases a handle twice or calls the engine during destruction.
            projection.release();Log("GC cleanup exception; transaction retained until process exit");
        }
    }
    static Invocation* Begin(Arguments& args) noexcept {
        const auto state=std::atomic_load(&Route().current);
        if(!state || state->thread.load()!=GetCurrentThreadId() || !args.context)return nullptr;
        auto& s=*state;std::unique_ptr<Invocation> invocation;
        try {
            GameEntity* entity=nullptr;
            if(!native::Call(s.caster,&entity,args.context).returned)throw std::runtime_error("caster lookup failed; original inputs forwarded");
            if(!entity || !s.owns(entity))return nullptr;
            invocation=std::make_unique<Invocation>();invocation->state=state;invocation->entity=entity;invocation->started=GetTickCount64();
            invocation->file="<inline>";
            if(args.file) {const auto name=s.strings.ReadText(args.file);invocation->file=name.read.returned?name.utf8:"<unread>";}
            invocation->projection=std::make_unique<detail::ManagedConfigProjection>(s.methods,s.metadata,s.objects,s.references,s.arrays,
                [&s](void* value,const auto& type){return s.schema.Describe(value,type);});
            invocation->result=invocation->projection->Apply({args.events,args.behaviors,args.groups});
            // Publish the complete private graph atomically at the call boundary.
            // Logic, Context, registration and source filename stay unchanged.
            args.events=invocation->result.roots[0];args.behaviors=invocation->result.roots[1];args.groups=invocation->result.roots[2];
            return invocation.release();
        }catch(const std::exception& e) {
            s.Log(std::string("projection failed; original inputs forwarded: ")+e.what()+
                (invocation?" source="+invocation->file:std::string()));
        }catch(...) {s.Log("projection failed; original inputs forwarded: unknown exception");}
        if(invocation)s.Close(invocation->projection);
        return nullptr;
    }
    static void Finish(Invocation* raw,bool abnormal) noexcept {
        if(!raw)return;
        std::unique_ptr<Invocation> invocation(raw);auto& s=*invocation->state;
        try {
            std::ostringstream line;line<<"entity="<<invocation->entity<<" inspected="<<invocation->result.inspected
                <<" copied="<<invocation->result.copied<<" disabled="<<invocation->result.disabled
                <<" source="<<invocation->file
                <<" original_returned="<<!abnormal<<" elapsed_ms="<<(GetTickCount64()-invocation->started);
            s.Log(line.str());
        }catch(...) {}
        // Official constructors now own references to adopted configs. Even
        // on an exception, any partially constructed managed objects retain
        // their own references; temporary input roots can be released.
        s.Close(invocation->projection);
    }
    static void __fastcall Detour(void* receiver,void* events,void* behaviors,void* groups,void* logic,
        void* context,void* registration,void* file,void* metadata) {
        Arguments args{receiver,events,behaviors,groups,logic,context,registration,file,metadata};
        auto* invocation=Begin(args);
        // A POD-only SEH finally preserves the original exception and runs
        // cleanup for native/managed SEH as well as C++ unwind. Do not convert
        // a failed official constructor into a successful empty registration.
        __try {
            reinterpret_cast<Original>(Route().hook.Original(0))(args.receiver,args.events,args.behaviors,args.groups,
                args.logic,args.context,args.registration,args.file,args.metadata);
        } __finally {Finish(invocation,AbnormalTermination()!=FALSE);}
    }
};
struct AnimationEventPolicy::Holder {std::shared_ptr<State> state;};
AnimationEventPolicy::AnimationEventPolicy(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,TypeRelationships& r,Fields f,
    ReferenceMethods refs,Owns owns,Trace trace,std::vector<std::string> disabled,std::vector<SelfPerformerRule> self_performers) {
    auto& router=State::Route();std::lock_guard<std::mutex> lock(router.install);
    if(std::atomic_load(&router.current))throw std::logic_error("event construction policy already owned");
    auto state=std::make_shared<State>(m,d,o,r,std::move(f),refs,std::move(owns),std::move(trace),std::move(disabled),std::move(self_performers));
    if(!router.hook.Pending()) {
        const auto method=m.Resolve("","HHECHLOIEJO","FGAHFIBMPCP","System.Void",{
            "RPG.GameCore.SingleAnimEventConfig[]","RPG.GameCore.SingleAnimBehaviourEvent[]","RPG.GameCore.AnimGroupEventConfig[]",
            "RPG.GameCore.LogicAnimEventEntry[]","RPG.GameCore.TaskContext","LDMKDJJLOIL","System.String"});
        router.hook.Install({{"animation event configuration construction",method.code,reinterpret_cast<void*>(&State::Detour)}});
    }
    holder_=new Holder{state};
    std::atomic_store(&router.current,std::move(state));
}
AnimationEventPolicy::~AnimationEventPolicy(){Stop();}
void AnimationEventPolicy::ActivateOnCurrentThread() {
    if(!holder_)return;
    auto& state_=holder_->state;
    const auto current=GetCurrentThreadId();DWORD empty=0;
    if(!state_->thread.compare_exchange_strong(empty,current) && empty!=current)
        throw std::logic_error("event construction policy cannot migrate engine threads");
}
void AnimationEventPolicy::Stop() noexcept {
    if(!holder_)return;
    try {
        auto& state_=holder_->state;
        auto& router=State::Route();std::lock_guard<std::mutex> lock(router.install);
        if(std::atomic_load(&router.current)==state_)std::atomic_store(&router.current,std::shared_ptr<State>{});
        state_->thread.store(0);
        if(!state_->pending.empty())router.unresolved.push_back(state_);
        delete holder_;holder_=nullptr;
    }catch(...) {
        // A stopped retained holder has no engine callbacks; do not lose its
        // failed-cleanup receipt just to make native destruction look complete.
        holder_->state->thread.store(0);
    }
}
}
