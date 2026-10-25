#include "CatalogReader.hpp"
#include <type_traits>

namespace hksr::sdk_v1 {
HitAnimationDefaults OfficialCatalog::ReadHitAnimationDefaults() {
    HitAnimationDefaults out;auto& s=*state_;State::Scope scope(s);
    void* config=nullptr;std::vector<DeclaredField> fields;
    try {config=s.GlobalVisualConfig(scope);fields=s.metadata.ReadObjectFields(config);}
    catch(const std::exception& error) {
        out.resilience_weights.error=out.avatar_weight.error=out.monster_weight.error=out.minimum_frame_interval.error=error.what();
        return out;
    }
    auto scalar=[&](auto& target,const char* name,const char* type) {
        try {target.value=Read<std::decay_t<decltype(target.value)>>(config,Field(fields,name,type));target.available=true;}
        catch(const std::exception& error){target.error=error.what();}
    };
    scalar(out.avatar_weight,"AvatarHitAdditiveWeightBase","System.Single");
    scalar(out.monster_weight,"MonsterHitAdditiveWeightBase","System.Single");
    scalar(out.minimum_frame_interval,"HitFrameMiniInterval","System.Int32");
    try {
        auto* array=Read<void*>(config,Field(fields,"ResilienceHitAdditiveWeights","System.Single[]"));
        Require(array,"hit resilience weight array is null");scope.Keep({{true,0},array});
        const auto count=s.arrays.Length(array);Require(count.read.returned && count.value>=0,"hit weight count unread");
        std::vector<float> values;
        for(int i=0;i<count.value;++i) {
            State::Scope item_scope(s);auto* item=item_scope.Keep(s.arrays.Get(array,i));
            values.push_back(Read<float>(item,Field(s.metadata.ReadObjectFields(item),"m_value","System.Single")));
        }
        out.resilience_weights.value=std::move(values);out.resilience_weights.available=true;
    }catch(const std::exception& error){out.resilience_weights.error=error.what();}
    return out;
}
}
