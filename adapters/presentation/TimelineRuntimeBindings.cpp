#include "TimelineRuntimeBindings.hpp"
#include <hksr/native/Invocation.hpp>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields, const char* owner, const char* name, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace.empty() && f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::runtime_error(std::string("Timeline field declaration differs: ") + owner + "." + name);
            found = &f;
        }
    if (!found)
        throw std::runtime_error(std::string("Timeline field absent: ") + owner + "." + name);
    return found->offset;
}
}
TimelineRegistryBindings ResolveTimelineRegistryBindings(const TimelineRegistryAccess::Fields& fields) {
    TimelineRegistryBindings out;
    const auto registry = fields("", "HPPNAAGGAMP"), record = fields("", "EBMMGODFEEO");
    out.list = Field(registry, "HPPNAAGGAMP", "MKDJOLKJJON", "System.Collections.Generic.List<EBMMGODFEEO>");
    out.dictionary = Field(registry, "HPPNAAGGAMP", "PIJENJPJFLL",
                           "System.Collections.Generic.Dictionary<UnityEngine.Playables.PlayableDirector,EBMMGODFEEO>");
    out.director = Field(record, "EBMMGODFEEO", "LDALFGDPLLF", "UnityEngine.Playables.PlayableDirector");
    out.resource = Field(record, "EBMMGODFEEO", "DKMKBPLKNDC", "System.String");
    out.name = Field(record, "EBMMGODFEEO", "FEBCMBILJMP", "System.String");
    out.id = Field(record, "EBMMGODFEEO", "PHFMCACHFIJ", "System.Int32");
    out.phase = Field(record, "EBMMGODFEEO", "MEPFOEEGBEA", "EBMMGODFEEO.DirectorState");
    return out;
}
bool IsTimelineObject(const DeclaredType& type, TimelineObject role) {
    return type.name_space.empty() && type.name == (role == TimelineObject::Registry ? "HPPNAAGGAMP" : "EBMMGODFEEO");
}
TimelinePhase DecodeTimelinePhase(std::int32_t value) {
    // Current DirectorState, not a cast assuming future enum stability.
    switch (value) {
    case 0:
        return TimelinePhase::Available;
    case 1:
        return TimelinePhase::Preparing;
    case 2:
        return TimelinePhase::Playing;
    case 3:
        return TimelinePhase::Paused;
    case 4:
        return TimelinePhase::Stopped;
    default:
        throw std::runtime_error("unrecognized Timeline DirectorState: " + std::to_string(value));
    }
}
Method ResolveTimelineDirectorLookup(ObjectMetadata& metadata, void* dictionary) {
    const auto type = metadata.ReadObjectType(dictionary);
    MetadataResolver methods({type});
    return methods.Resolve(type.name_space.c_str(), type.name.c_str(), "TryGetValue", "System.Boolean",
                           {"UnityEngine.Playables.PlayableDirector", "EBMMGODFEEO&"});
}
ReferenceListMethods ResolveTimelineRecordList(const DeclaredType& type) {
    return ResolveReferenceListMethods(type, "EBMMGODFEEO");
}
CommonTimelineBindings ResolveCommonTimelineBindings(MethodResolver& m) {
    CommonTimelineBindings out;
    out.play = m.Resolve("", "NCEDGGKOMIH", "NOLKCBFDAOJ", "System.Int32",
                         {"System.UInt32", "System.String", "UnityEngine.Playables.PlayableAsset",
                          "UnityEngine.Vector3", "UnityEngine.Vector3",
                          "System.Action<UnityEngine.Playables.PlayableDirector,UnityEngine.Playables.PlayableAsset>",
                          "UnityEngine.Events.UnityAction", "System.Double", "UnityEngine.Playables.DirectorWrapMode"});
    out.stop = m.Resolve("", "EBMMGODFEEO", "HFKFNFKIEON", "System.Void", {});
    return out;
}
TimelineReflectedField ResolveCommonTimelineManager(MethodResolver& methods, ObjectMetadata& metadata,
                                                    ManagedObjects& objects, const TimelineRetain& retain) {
    TimelineReflectedField out;
    auto* type = retain(objects.ManagedType("RPG.Client", "GlobalVars"));
    auto* name = retain(objects.NewStringUtf8("s_TimelineManager"));
    const auto get = methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                     {"System.String", "System.Reflection.BindingFlags"});
    ObjectResult result;
    result.call = native::Call(get, &result.object, type, name, 56);
    auto* field = retain(result);
    const auto actual = metadata.ReadObjectType(field);
    MetadataResolver resolver({actual});
    out.get = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "GetValue", "System.Object",
                               {"System.Object"});
    out.object = field;
    return out;
}
namespace {
constexpr const char* HistoryType =
    "System.Collections.Generic.List<System.Collections.Generic.KeyValuePair<System.String,System.Int32>>";
}
TimelineTrackingBindings ResolveTimelineTrackingBindings(MethodResolver& m, const TimelineRegistryAccess::Fields& f) {
    TimelineTrackingBindings out;
    const auto registry = f("", "HPPNAAGGAMP"), record = f("", "EBMMGODFEEO");
    out.fields = [registry, record](const char*, const char* type) {
        if (std::string(type) == "HPPNAAGGAMP")
            return registry;
        if (std::string(type) == "EBMMGODFEEO")
            return record;
        throw std::logic_error("unexpected Timeline schema request");
    };
    out.clone = m.Resolve("System", "Object", "MemberwiseClone", "System.Object", {});
    unsigned found = 0;
    for (const auto& field : registry)
        if (field.owner_namespace.empty() && field.owner_name == "HPPNAAGGAMP" && field.name == "DFNAOFNEBHC") {
            if (field.type != HistoryType || !field.flags_observed || (field.flags & 0x10))
                throw std::runtime_error("Timeline history field declaration differs");
            out.history_offset = field.offset;
            ++found;
        }
    if (found != 1)
        throw std::runtime_error("Timeline history field absent or ambiguous");
    found = 0;
    for (const auto& field : registry)
        if (field.owner_namespace.empty() && field.owner_name == "HPPNAAGGAMP" && field.name == "MKDJOLKJJON") {
            if (field.type != "System.Collections.Generic.List<EBMMGODFEEO>" || !field.flags_observed ||
                (field.flags & 0x10))
                throw std::runtime_error("Timeline query list declaration differs");
            out.query_offset = field.offset;
            ++found;
        }
    if (found != 1)
        throw std::runtime_error("Timeline query list absent or ambiguous");
    return out;
}
TimelineReflectedField ResolveTimelineRegistryField(MethodResolver& methods, ObjectMetadata& metadata,
                                                    ManagedObjects& objects, TimelineRegistryField role,
                                                    const TimelineRetain& retain) {
    TimelineReflectedField out;
    auto* type = retain(objects.ManagedType("", "HPPNAAGGAMP"));
    auto* name = retain(objects.NewStringUtf8(role == TimelineRegistryField::History ? "DFNAOFNEBHC" : "MKDJOLKJJON"));
    const auto getter = methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                        {"System.String", "System.Reflection.BindingFlags"});
    ObjectResult result;
    result.call = native::Call(getter, &result.object, type, name, 54);
    auto* field = retain(result);
    const auto actual = metadata.ReadObjectType(field);
    MetadataResolver resolver({actual});
    out.set = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                               {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                "System.Reflection.Binder", "System.Globalization.CultureInfo"});
    out.object = field;
    return out;
}
Method ResolveTimelineHistoryRange(const DeclaredType& actual) {
    MetadataResolver resolver({actual});
    return resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "GetRange", HistoryType,
                            {"System.Int32", "System.Int32"});
}
Method ResolveTimelineRecordRange(const DeclaredType& actual) {
    MetadataResolver resolver({actual});
    return resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "GetRange",
                            "System.Collections.Generic.List<EBMMGODFEEO>", {"System.Int32", "System.Int32"});
}
Method ResolveTimelineRecordRemove(const DeclaredType& actual) {
    MetadataResolver resolver({actual});
    return resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "RemoveAt", "System.Void",
                            {"System.Int32"});
}
namespace {
constexpr std::array<const char*, 2> wait_types{{"JODBIGDDMOB", "IGBEIECLHMC"}};
}
std::array<TimelineWaitBinding, 2> ResolveTimelineWaitBindings(const TimelineRegistryAccess::Fields& declarations) {
    std::array<TimelineWaitBinding, 2> out;
    for (std::size_t i = 0; i < wait_types.size(); ++i) {
        try {
            bool found = false;
            for (const auto& f : declarations("", wait_types[i]))
                if (f.owner_namespace.empty() && f.owner_name == wait_types[i] && f.name == "BMHGECJDFGN") {
                    if (found || f.type != "EBMMGODFEEO" || !f.flags_observed || (f.flags & 0x10))
                        throw std::runtime_error("wait saved-record declaration differs");
                    out[i].offset = f.offset;
                    found = true;
                }
            if (!found)
                throw std::runtime_error("wait saved-record declaration absent");
            out[i].available = true;
        } catch (const std::exception& e) {
            out[i].error = std::string(wait_types[i]) + ": " + e.what();
        }
    }
    return out;
}
TimelineReflectedField ResolveTimelineWaitField(MethodResolver& methods, ObjectMetadata& metadata,
                                                ManagedObjects& objects, std::size_t index,
                                                const TimelineRetain& retain) {
    const auto type = retain(objects.ManagedType("", wait_types.at(index)));
    auto* name = retain(objects.NewStringUtf8("BMHGECJDFGN"));
    const auto getter = methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                        {"System.String", "System.Reflection.BindingFlags"});
    ObjectResult value;
    value.call = native::Call(getter, &value.object, type, name, 54);
    auto* field = retain(value);
    const auto actual = metadata.ReadObjectType(field);
    MetadataResolver resolver({actual});
    const auto setter = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                         {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                          "System.Reflection.Binder", "System.Globalization.CultureInfo"});
    return {field, {}, setter};
}
}
