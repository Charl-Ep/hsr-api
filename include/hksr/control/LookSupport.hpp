#pragma once
#include <hksr/unity/AssetLeases.hpp>
#include <hksr/unity/OwnedUnityObjects.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/unity/SceneObjects.hpp>
#include <hksr/model/ArtAdmission.hpp>
#include <hksr/unity/ComponentConfig.hpp>

namespace hksr::sdk_v1 {
struct LookSupportServices {
    ManagedObjects& objects;
    const SceneObjects& scene;
    const ManagedArrays& arrays;
    const VisualObjects& visuals;
    const ComponentConfig& config;
};
struct LookSupportStatus {
    bool started=false,existing=false,not_authored=false,prepared=false;
    bool add_attempted=false,rollback_needed=false,rollback_observed=false,rollback_settled=false,closed=false;
    ObjectResult addition;
    CallResult overwrite;
    std::size_t remapped_references=0;
    std::vector<std::string> errors;
};
struct LookSupportResult {bool complete=false;std::vector<std::string> errors;};
// Prepare on a PRIVATE, inactive Art before official admission. All animation,
// targeting, constraints and IK remain official. No player donor and no manual
// pose evaluation. Parent/official lifecycle owns successful components.
class HKSR_API LookSupport {
public:
    LookSupport(ReferenceMethods,UnityLifetimeMethods);
    ~LookSupport(); // explicit Close; destructor never calls the game
    LookSupport(const LookSupport&)=delete;
    LookSupport& operator=(const LookSupport&)=delete;
    void Prepare(LookSupportServices services,void* private_art,const std::string& npc_prefab_path);
    // Failed Add may have changed the hierarchy before throwing. Retire those
    // components and wait for actual native death before activating that Art.
    bool SettleFailure();
    bool CanSubmit() const;
    LookSupportResult Close();
    const LookSupportStatus& Status() const;
private:
    struct State;State* state_;
};
}
