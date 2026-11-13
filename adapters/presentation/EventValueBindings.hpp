#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
namespace hksr::api::detail {
enum class EventValueField {
    Numeric,
    Strings,
    GlobalConfiguration,
    GlobalNumericDefaults,
    CharacterNumericDefaults,
    CharacterStringDefaults
};
struct EventValueFieldBinding {
    void* object = nullptr;
    Method get, set;
};
struct EventValueMethods {
    Method context, owner, stage_ctor, merge_numeric, merge_strings, load_character;
};
EventValueMethods ResolveEventValueMethods(MethodResolver&);
EventValueFieldBinding ResolveEventValueField(MethodResolver&, ObjectMetadata&, ManagedObjects&, EventValueField,
                                              const std::function<void*(ObjectResult)>& retain);
ObjectResult AllocateEventValueStage(ManagedObjects&);
Method ResolveEventStringDictionaryCopy(ObjectMetadata&, void* dictionary);
}
