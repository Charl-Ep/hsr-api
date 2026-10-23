#include <hksr/presentation/movement/AnimationMovementData.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <limits>
#include <set>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
struct TargetSample {Position3 position;float percentage=1,distance=0;Position3 direction;};
static_assert(sizeof(Position3)==12 && sizeof(TargetSample)==32);
}
struct AnimationMovementData::State {
    AnimationMoveDataServices services;ManagedRoots roots;std::vector<ManagedRootId> held;
    std::set<void*> allocations,retained;CompiledAnimationMove plan;
    void* object=nullptr;bool attempted=false,submitted=false,ready=false,closed=false,release_reported=false;std::string error;
    explicit State(AnimationMoveDataServices s):services(std::move(s)),roots(services.references){}
    void Keep(void* value) {
        if(retained.count(value))return;
        held.reserve(held.size()+1);held.push_back(roots.Keep(value));
        if(!roots.Status(held.back()).ready)throw std::runtime_error("movement data retention failed");
        retained.insert(value);
    }
    void* New(const std::function<ObjectResult()>& allocate) {
        const auto value=allocate();
        if(!value.call.returned || !value.object)throw std::runtime_error("movement data allocation failed");
        Keep(value.object);
        if(!allocations.insert(value.object).second)throw std::runtime_error("movement allocator reused a live object");
        return value.object;
    }
    template<class... A>void Call(Method method,A... args) {
        const auto receipt=native::CallVoid(method,args...);
        if(!receipt.returned)throw std::runtime_error("movement data constructor/list call failed: "+std::to_string(receipt.exception));
    }
    template<class T>void Write(void* target,std::size_t offset,const T& value) {
        // Only newly allocated, unpublished objects. Reference children are
        // independently rooted until Close; no existing managed graph is edited.
        if(!allocations.count(target) || !native::CopyMemoryChecked(static_cast<char*>(target)+offset,&value,sizeof(value)))
            throw std::runtime_error("private movement field write failed");
    }
};
AnimationMovementData::AnimationMovementData(AnimationMoveDataServices services):state_(std::make_unique<State>(std::move(services))) {
    const auto& s=state_->services;
    if(!s.data || !s.segment || !s.list || !s.data_ctor.code || !s.segment_ctor.code || !s.list_ctor.code || !s.list_add.code)
        throw std::invalid_argument("movement data services incomplete");
}
AnimationMovementData::~AnimationMovementData()=default;
bool AnimationMovementData::Prepare(const AnimationMovePlan& input) {
    auto& s=*state_;if(s.attempted || s.closed)return false;s.attempted=true;
    try {
        s.plan=CompileAnimationMove(input);
        if(s.plan.segments.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()))throw std::invalid_argument("too many movement segments");
        // Curves can outlive an animation config resource. Retain before the
        // first allocation/call, not after publishing them into a segment.
        for(const auto& part:s.plan.segments)if(part.curve)s.Keep(part.curve);
        auto* data=s.New(s.services.data);s.Call(s.services.data_ctor,data);
        auto* list=s.New(s.services.list);s.Call(s.services.list_ctor,list,static_cast<int>(s.plan.segments.size()));
        const auto& l=s.services.layout;
        for(const auto& part:s.plan.segments) {
            auto* segment=s.New(s.services.segment);s.Call(s.services.segment_ctor,segment);
            s.Write(segment,l.curve,part.curve);s.Write(segment,l.exit_time,part.exit_time);
            s.Write(segment,l.origin,part.origin);s.Write(segment,l.target_sample,TargetSample{part.target,1,0,{}});
            s.Write(segment,l.segment_start,part.start);s.Write(segment,l.segment_end,part.end);
            // Endpoints already include the caller's private return anchor.
            // Mode 1 would re-read battle formation and replace those endpoints.
            s.Write(segment,l.update_mode,std::int32_t{0});
            s.Call(s.services.list_add,list,segment);
        }
        s.Write(data,l.segments,list);s.Write(data,l.previous_time,0.0f);s.Write(data,l.duration,s.plan.duration);
        s.Write(data,l.end_time,s.plan.end_time);s.Write(data,l.key,s.plan.animation_key);
        s.Write(data,l.progress,0.0f);s.Write(data,l.force_start,s.plan.force_start);
        s.object=data;s.ready=true;return true;
    }catch(const std::exception& e){s.error=e.what();Close();return false;}
}
void* AnimationMovementData::Object()const{return state_->ready && !state_->closed?state_->object:nullptr;}
bool AnimationMovementData::BeginSubmission()const {
    if(!Object() || state_->submitted)return false;
    state_->submitted=true;return true;
}
const CompiledAnimationMove& AnimationMovementData::Plan()const{return state_->plan;}
const std::string& AnimationMovementData::Error()const{return state_->error;}
bool AnimationMovementData::Close() {
    auto& s=*state_;s.ready=false;
    for(auto it=s.held.begin();it!=s.held.end();) {
        if(s.roots.Release(*it).complete){s.roots.Forget(*it);it=s.held.erase(it);}else ++it;
    }
    s.closed=s.held.empty();
    if(!s.closed && !s.release_reported) {
        if(!s.error.empty())s.error+="; ";
        s.error+="movement managed-root release incomplete";s.release_reported=true;
    }
    return s.closed;
}
}
