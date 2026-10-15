#include "CatalogReader.hpp"

namespace hksr::sdk_v1 {
ReadyAnimationDefaults OfficialCatalog::ReadReadyAnimationDefaults() {
    ReadyAnimationDefaults out;auto& s=*state_;State::Scope scope(s);
    auto text=[&](const char* field,TextInput& target) {
        try {
            State::Scope local(s);
            const auto value=s.StaticValue(local,"",detail::catalog_bindings::animation_names,field);
            Require(value.call.returned,"official ready constant read failed");
            target.value=value.object?s.Text(local.Keep(value)):std::string();target.available=true;
        }catch(const std::exception& e){target.error=e.what();}
    };
    // Current declarations + PID165512 values, independently read. A missing
    // layer constant does not invalidate transitions or other valid constants.
    text(detail::catalog_bindings::default_expression,out.default_expression);text(detail::catalog_bindings::empty_layer,out.empty_layer_state);
    text(detail::catalog_bindings::hit_additive_layer,out.hit_additive_layer);text(detail::catalog_bindings::hit_override_layer,out.hit_override_layer);
    try {
        void* config=s.GlobalVisualConfig(scope);
        out.transition_in={true,Read<float>(config,Field(s.metadata.ReadObjectFields(config),"SkillReadyAnimDefaultTransitDuration","System.Single")),{}};
    }catch(const std::exception& e){out.transition_in.error=e.what();}
    // BCCA800 normal branch: six directed fallback pairs. Custom edges are
    // unbounded and live in each character document. Names stay official.
    for(int from=0;from<3;++from)for(int to=0;to<3;++to)if(from!=to) {
        ReadyTransitionInput row;row.source_index=static_cast<int>(out.transitions.size());
        row.from={true,from+1,{}};row.to={true,to+1,{}};
        row.state=ReadDefaultReadyTransition(from,to);row.transition_in=out.transition_in;
        row.reverse_outro={true,0,{}};out.transitions.push_back(std::move(row));
    }
    return out;
}
TextInput OfficialCatalog::ReadDefaultReadyTransition(std::int32_t from,std::int32_t to) {
    TextInput output;auto& s=*state_;State::Scope scope(s);
    try {
        const auto query=s.methods.Resolve("",detail::catalog_bindings::animation_names,detail::catalog_bindings::ready_transition,"System.String",
            {"System.Int32","System.Int32"},MethodKind::Static);
        const auto result=s.Call(query,from,to);
        Require(result.call.returned,"official default ready transition query failed");
        if(result.object)output.value=s.Text(scope.Keep(result));
        output.available=true;
    }catch(const std::exception& e){output.error=e.what();}
    return output;
}
}
