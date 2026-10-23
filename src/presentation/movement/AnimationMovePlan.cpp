#include <hksr/presentation/movement/AnimationMovementData.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace hksr::sdk_v1 {
namespace {
bool Finite(Position3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
float Narrow(double value) {
    if(!std::isfinite(value) || value>std::numeric_limits<float>::max())
        throw std::invalid_argument("movement duration is not representable");
    return static_cast<float>(value);
}
}
CompiledAnimationMove CompileAnimationMove(const AnimationMovePlan& plan) {
    if(!Finite(plan.origin) || plan.segments.empty())throw std::invalid_argument("movement needs a finite origin and segments");
    if(plan.clock!=MovementClock::Animation && plan.clock!=MovementClock::DistanceSpeed)
        throw std::invalid_argument("unknown movement clock");
    CompiledAnimationMove out;out.animation_key=plan.animation_key;out.force_start=plan.force_start;
    auto origin=plan.origin;double seconds=0;
    for(const auto& source:plan.segments) {
        if(!Finite(source.target) || !std::isfinite(source.exit_time))
            throw std::invalid_argument("movement target/exit time is not finite");
        CompiledMoveSegment segment{origin,source.target,source.start,source.end,source.exit_time,source.curve};
        if(plan.clock==MovementClock::Animation) {
            if(!std::isfinite(source.start) || !std::isfinite(source.end) || source.end<=source.start)
                throw std::invalid_argument("movement interval must have a positive finite span");
            if(source.end>out.end_time)out.end_time=source.end;
        }else {
            if(!std::isfinite(source.speed) || source.speed<=0)throw std::invalid_argument("movement speed must be positive and finite");
            const double x=double(source.target.x)-origin.x,y=double(source.target.y)-origin.y,z=double(source.target.z)-origin.z;
            const double duration=std::sqrt(x*x+y*y+z*z)/source.speed;
            if(duration<=0)throw std::invalid_argument("zero-distance speed segment requires an explicit animation-time interval");
            segment.start=Narrow(seconds);seconds+=duration;segment.end=Narrow(seconds);
        }
        out.segments.push_back(segment);origin=source.target;
    }
    if(plan.clock==MovementClock::DistanceSpeed) {
        out.duration=Narrow(seconds);out.end_time=1;
        if(out.duration<=0)throw std::invalid_argument("movement duration underflows native precision");
        for(auto& segment:out.segments) {
            segment.start/=out.duration;segment.end/=out.duration;
            if(segment.end<=segment.start)throw std::invalid_argument("movement interval underflows native precision");
        }
    }
    return out;
}
}
