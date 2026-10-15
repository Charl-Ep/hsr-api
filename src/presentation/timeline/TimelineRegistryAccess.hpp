#pragma once
#include "TimelineOwnership.hpp"
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
#include <memory>

namespace hksr::sdk_v1::detail {
// Current-domain adapter for the audited registry/record contracts. Reads only;
// no Director scheduling, guessed generic collection layout or reference writes.
// Keep this reader alive through a boundary, then Close on the engine thread.
// Pointers in a snapshot are valid only until Close unless adopted elsewhere.
struct TimelineRegistryRecord {
    TimelineRecordView view;
    void* object=nullptr;
    void* director=nullptr;
    void* resource=nullptr;
    void* runtime_name=nullptr;
    bool resource_is_null=false;
};
struct TimelineRegistryRows {
    std::vector<TimelineRegistryRecord> records;
    std::vector<std::string> errors;
    bool complete=true;
};
class TimelineRegistryAccess {
public:
    using Fields=std::function<std::vector<DeclaredField>(const char*,const char*)>;
    TimelineRegistryAccess(MethodResolver&,ObjectMetadata&,ReferenceMethods,Fields);
    ~TimelineRegistryAccess();
    TimelineRegistryAccess(const TimelineRegistryAccess&)=delete;
    TimelineRegistryAccess& operator=(const TimelineRegistryAccess&)=delete;
    // Hold original managed arguments before any allocation in a detour.
    void Hold(void* object);
    // Exact official Director -> record dictionary, not a list/ID/name guess.
    // null means a successful dictionary miss; failures throw distinctly.
    void* Find(void* registry,void* director);
    TimelineRegistryRecord Read(void* registry,void* record);
    // Preserves actual list order; a bad row is reported without suppressing
    // other readable rows. Used for filtered lookup or partial-borrow diagnosis.
    TimelineRegistryRows Rows(void* registry);
    bool Close();
private:
    struct State;
    std::unique_ptr<State> state_;
};
}
