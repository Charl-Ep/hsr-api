#pragma once
#include <hksr/presentation/events/AnimationExitConfig.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/native/Invocation.hpp>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>

namespace hksr::api::detail {
// Official configuration translation only. The caller retains every object
// produced while reading/cloning the source and owns the release boundary.
class AnimationExitSchema {
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    TypeRelationships& relationships;
    ManagedArrays arrays;
    std::function<void*(ObjectResult, bool)> retain_;
    std::string& operation;
    struct Field {
        void* object;
        Method get, set;
    };
    std::map<std::string, Field> fields;
    std::map<std::string, void*> abilities;

  public:
    AnimationExitSchema(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, TypeRelationships& t, ManagedArrays a,
                        std::function<void*(ObjectResult, bool)> retain, std::string& context)
        : methods(m), metadata(d), objects(o), relationships(t), arrays(std::move(a)), retain_(std::move(retain)),
          operation(context) {}
    bool ConfigReference(const std::string& name);
    static void Require(bool ok, const char* why) {
        if (!ok)
            throw std::runtime_error(why);
    }
    void* Keep(ObjectResult r, bool nullable = false) {
        return retain_(r, nullable);
    }
    template <class... A> void* Object(Method m, A... a) {
        ObjectResult r;
        r.call = native::Call(m, &r.object, a...);
        return Keep(r, true);
    }
    template <class... A> void Call(Method m, A... a) {
        const auto r = native::CallVoid(m, a...);
        if (!r.returned)
            throw std::runtime_error("managed call failed; exception=" + std::to_string(r.exception));
    }
    void* Text(const std::string& text) {
        return Keep(objects.NewStringUtf8(text.c_str()));
    }
    Field& Resolve(const std::string& owner, const std::string& name);
    void* Get(const std::string& owner, const std::string& name, void* object);
    void Set(const char* owner, const char* name, void* object, void* value);
    void* Scalar(const char* type, const char* value);
    void* New(const char* type);
    void* Array(const char* type, const std::vector<void*>& values);
    void* Source(const EffectRemovalSource& source);
    void* Build(const std::string& name, const std::vector<void*>& tasks);
};
}
