#pragma once
#include "../entity/EntityAccess.hpp"
#include "../managed/ManagedRoots.hpp"
#include <memory>

namespace hksr::sdk_v1 {
struct CharacterOutfitStatus {
    bool started=false,applicable=false,applied=false,restore_attempted=false,closed=false;
    std::uint32_t avatar=0;
    int gender=0,base_type=0;
    bool config_refresh_required=false;
    CallResult apply,refresh,restore;
    std::vector<std::string> errors;
};
// One owned entity epoch. Uses official outfit composition and OverrideOutfit;
// never changes account dress IDs, player gender, entity type, bones or meshes.
// Close(true) restores the captured override on a live owner frame. After the
// owner is disposed, Close(false) only releases managed roots.
class CharacterOutfit {
public:
    HKSR_API CharacterOutfit(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods,EntityAccessMethods);
    HKSR_API ~CharacterOutfit();
    CharacterOutfit(const CharacterOutfit&)=delete;
    CharacterOutfit& operator=(const CharacterOutfit&)=delete;
    HKSR_API void Prepare(GameEntity* owned_entity,std::uint32_t avatar);
    HKSR_API bool Close(bool owner_alive);
    HKSR_API const CharacterOutfitStatus& Status() const;
private:
    struct State;std::unique_ptr<State> state_;
};
}
