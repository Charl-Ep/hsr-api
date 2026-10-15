#pragma once
#include "../managed/ManagedRoots.hpp"
#include <hksr/model/ProfileInputs.hpp>
#include <hksr/model/ModelProfile.hpp>
#include <hksr/managed/ManagedArrays.hpp>

namespace hksr::sdk_v1 {
struct ModelProfileResult { bool complete=false; std::vector<std::string> errors; };
struct ProfileDescription {
    std::string model_path;
    std::vector<std::string> event_files,excluded_layers;
    std::string character_config;
};
struct ModelProfileInputStatus {
    bool started=false,prepared=false,closed=false;
    ObjectResult operation;
    CallResult initialize;
    ModelInputs baseline,target;
    void* baseline_entity_config=nullptr;
    void* model=nullptr;
    std::string character_config;
    std::vector<std::string> errors;
};
// Owns immutable inputs, NOT a running operation. No Begin/Cancel/automatic
// restoration in Close or destructor. The request owner must end/cancel use
// before closing these roots. Baseline arrays are retained, not recreated.
class HKSR_API ModelProfileInputs {
public:
    explicit ModelProfileInputs(ReferenceMethods refs);
    ~ModelProfileInputs();
    ModelProfileInputs(const ModelProfileInputs&) = delete;
    ModelProfileInputs& operator=(const ModelProfileInputs&) = delete;
    void Prepare(ManagedObjects& objects,const ManagedArrays& arrays,const ProfileInputs& access,
        ProfileEventFields event_fields,void* owned_model,void* owned_event_consumer,const ProfileDescription& target);
    ModelProfileResult Close();
    const ModelProfileInputStatus& Status() const;
private:
    struct State;
    State* state_;
};
}
