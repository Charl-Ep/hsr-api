#pragma once
#include "../../metadata/RuntimeMetadata.hpp"
#include <functional>

namespace hksr::sdk_v1 {
// One owned model-profile lease. Does not create/attach game components or
// execute ability/AI initialization. Caller supplies the current event component.
class HKSR_API EventValueLease {
public:
    virtual ~EventValueLease()=default;
    virtual bool Apply(void* event_component,const std::string& character_config)=0;
    virtual bool Restore()=0;
    virtual bool Close(bool owner_ended)=0;
    virtual const std::string& Error() const=0;
};
class HKSR_API EventValueInputs final:public EventValueLease {
public:
    EventValueInputs(MethodResolver&,ObjectMetadata&,ManagedObjects&,ReferenceMethods);
    ~EventValueInputs();
    EventValueInputs(const EventValueInputs&)=delete;
    EventValueInputs& operator=(const EventValueInputs&)=delete;
    bool Apply(void*,const std::string&) override;
    bool Restore() override;
    bool Close(bool owner_ended) override;
    const std::string& Error() const override;
private:
    struct State;State* state_;
};
}
