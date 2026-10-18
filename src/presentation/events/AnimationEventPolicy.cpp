#include <hksr/presentation/events/AnimationEventPolicy.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include "AnimationConfigSchema.hpp"
#include "AnimationConstructionRoute.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <Windows.h>
#include <atomic>
#include <sstream>

namespace hksr::sdk_v1 {
struct AnimationEventPolicy::State {
    using Arguments=detail::AnimationConstructionArguments;
    MethodResolver& methods;ObjectMetadata& metadata;ManagedObjects& objects;
    ReferenceMethods references;ArrayMethods arrays;Method caster;
    ComponentConfig strings;
    std::vector<SelfPerformerRule> self_performers;
    detail::AnimationConfigSchema schema;
    Owns owns;Trace trace;
    std::atomic<DWORD> thread{0};
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
            // This transaction can finish after its subscription was stopped.
            // Keep unresolved handles independently of the subscriber lifetime.
            projection.release();Log("GC cleanup incomplete; receipt retained until process exit");
        }catch(...) {
            // Keep an existing owner even if bookkeeping allocation failed.
            // Emergency retention leaks only on OOM/cleanup failure, never
            // releases a handle twice or calls the engine during destruction.
            projection.release();Log("GC cleanup exception; transaction retained until process exit");
        }
    }
    static Invocation* BeginClient(const std::shared_ptr<State>& state,Arguments& args) noexcept {
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
            for(const auto& unresolved:invocation->result.uninspected) {
                std::ostringstream line;line<<"uninspected branch retained source="<<invocation->file
                    <<" object="<<unresolved.source<<" error="<<unresolved.error;
                s.Log(line.str());
            }
            // Publish the rewired graph atomically. Uninspected branches keep
            // their original references and remain explicitly unqualified.
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
        if(raw) {
            std::unique_ptr<Invocation> invocation(raw);auto& s=*invocation->state;
            try {
                std::ostringstream line;line<<"entity="<<invocation->entity<<" inspected="<<invocation->result.inspected
                    <<" copied="<<invocation->result.copied<<" disabled="<<invocation->result.disabled
                    <<" uninspected="<<invocation->result.uninspected.size()
                    <<" source="<<invocation->file
                    <<" original_returned="<<!abnormal<<" elapsed_ms="<<(GetTickCount64()-invocation->started);
                s.Log(line.str());
            }catch(...) {}
            // Official constructors now own references to adopted configs.
            // Partial managed objects retain theirs even after an exception.
            s.Close(invocation->projection);
        }
    }
};
struct AnimationEventPolicy::Holder {
    std::shared_ptr<State> state;
    std::unique_ptr<detail::AnimationConstructionSubscription> subscription;
};
AnimationEventPolicy::AnimationEventPolicy(MethodResolver& m,ObjectMetadata& d,ManagedObjects& o,TypeRelationships& r,Fields f,
    ReferenceMethods refs,Owns owns,Trace trace,std::vector<std::string> disabled,std::vector<SelfPerformerRule> self_performers) {
    auto state=std::make_shared<State>(m,d,o,r,std::move(f),refs,std::move(owns),std::move(trace),std::move(disabled),std::move(self_performers));
    auto holder=std::make_unique<Holder>();holder->state=state;
    holder->subscription=std::make_unique<detail::AnimationConstructionSubscription>(m,true,
        detail::AnimationConstructionParticipant{
            [state](auto& args)->void*{return State::BeginClient(state,args);},
            [](void* token,bool returned){State::Finish(static_cast<State::Invocation*>(token),!returned);}});
    holder_=holder.release();
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
        state_->thread.store(0);
        holder_->subscription->Stop();
        delete holder_;holder_=nullptr;
    }catch(...) {
        // A stopped retained holder has no engine callbacks; do not lose its
        // failed-cleanup receipt just to make native destruction look complete.
        holder_->state->thread.store(0);
    }
}
}
