#include "../../../adapters/presentation/EffectInputBindings.hpp"
#include <hksr/presentation/effects/EffectInputs.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>

namespace hksr::api {
using detail::EffectInputConventions;
using detail::EffectInputField;
using detail::EffectNullableRotation;
using detail::EffectNullableVector;
struct EffectInputs::State {
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ManagedRoots roots;
    std::vector<ManagedRootId> retained;
    struct Field {
        void* object = nullptr;
        Method call;
    };
    std::map<EffectInputField, Field> fields;
    detail::EffectInputFields entity_fields;
    Method attach;
    std::map<int, void*> flag_arrays;
    bool spatial_prepared = false;
    bool closed = false;
    State(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r)
        : methods(m), metadata(d), objects(o), roots(r) {}
    void* Keep(ObjectResult value) {
        if (!value.call.returned || !value.object)
            throw std::runtime_error("effect field object unavailable");
        const auto id = roots.Keep(value.object);
        retained.push_back(id);
        if (!roots.Status(id).ready)
            throw std::runtime_error("effect field retention failed");
        return value.object;
    }
    Field Get(EffectInputField key) {
        const auto identity = detail::DescribeEffectInputField(key);
        const auto* name = identity.name;
        const bool is_static = identity.is_static;
        if (const auto found = fields.find(key); found != fields.end())
            return found->second;
        auto* type = Keep(detail::ResolveEffectInputOwnerType(objects, key));
        auto* text = Keep(objects.NewStringUtf8(name));
        const auto find = methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                          {"System.String", "System.Reflection.BindingFlags"});
        ObjectResult found;
        found.call = native::Call(find, &found.object, type, text, is_static ? 56 : 52);
        auto* field = Keep(found);
        const auto actual = metadata.ReadObjectType(field);
        MetadataResolver resolver({actual});
        Field result;
        result.object = field;
        if (is_static)
            result.call = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "GetValue", "System.Object",
                                           {"System.Object"});
        else
            result.call = resolver.Resolve(actual.name_space.c_str(), actual.name.c_str(), "SetValue", "System.Void",
                                           {"System.Object", "System.Object", "System.Reflection.BindingFlags",
                                            "System.Reflection.Binder", "System.Globalization.CultureInfo"});
        fields.emplace(key, result);
        return result;
    }
    void PrepareFields(void* parameters, std::initializer_list<EffectInputField> requested) {
        bool complete = true;
        for (auto field : requested)
            if (!entity_fields.count(field))
                complete = false;
        if (complete)
            return;
        const auto resolved = detail::ResolveEffectInputFields(metadata, parameters, requested);
        entity_fields.insert(resolved.begin(), resolved.end());
    }
    void PrepareEntity(void* parameters) {
        PrepareFields(parameters,
                      {EffectInputField::PositionOffset, EffectInputField::RotationOffset, EffectInputField::Scale,
                       EffectInputField::FadeIn, EffectInputField::SimulateImmediately, EffectInputField::Replay,
                       EffectInputField::ParamUsage, EffectInputField::MaxPitch});
        if (!attach.code)
            attach = detail::ResolveEffectAttachmentSetter(methods);
    }
    void PrepareSpatial(void* parameters) {
        PrepareEntity(parameters);
        if (spatial_prepared)
            return;
        PrepareFields(parameters, {EffectInputField::InitialPosition, EffectInputField::AnchorPosition,
                                   EffectInputField::InitialRotation, EffectInputField::IgnorePrefabAttachment});
        spatial_prepared = true;
    }
    template <class T> void Scalar(void* parameters, EffectInputField name, const T& value) {
        // Only audited blittable fields on a private, unsubmitted parameter
        // object. Managed reference writes always use FieldInfo.SetValue.
        if (!native::CopyMemoryChecked(static_cast<unsigned char*>(parameters) + entity_fields.at(name).offset, &value,
                                       sizeof(value)))
            throw std::runtime_error(std::string("entity effect field write failed: ") +
                                     detail::DescribeEffectInputField(name).name);
    }
    CallResult Set(Field field, void* parameters, void* value) {
        return native::CallVoid(field.call, field.object, parameters, value, 0, static_cast<void*>(nullptr),
                                static_cast<void*>(nullptr));
    }
    void ReleaseValue(ManagedRootId id) {
        if (!id)
            return;
        if (roots.Release(id).complete) {
            roots.Forget(id);
            retained.erase(std::remove(retained.begin(), retained.end(), id), retained.end());
        }
    }
    void* FlagArray(int flag) {
        if (const auto it = flag_arrays.find(flag); it != flag_arrays.end())
            return it->second;
        auto* type = Keep(detail::ResolveEffectFlagType(objects));
        const auto type_root = retained.back();
        ManagedRootId value_root = 0, array_root = 0;
        try {
            const auto box = methods.Resolve("System", "Enum", "ToObject", "System.Object",
                                             {"System.Type", "System.Int32"}, MethodKind::Static);
            ObjectResult boxed;
            boxed.call = native::Call(box, &boxed.object, type, std::int32_t{flag});
            auto* value = Keep(boxed);
            value_root = retained.back();
            ManagedArrays arrays(ResolveArrayMethods(methods));
            auto* array = Keep(arrays.Create(type, 1));
            array_root = retained.back();
            if (!arrays.Set(array, value, 0).returned)
                throw std::runtime_error("effect resident array initialization failed");
            // Config tasks also share immutable Flags arrays with requests.
            flag_arrays.emplace(flag, array);
        } catch (...) {
            ReleaseValue(array_root);
            ReleaseValue(value_root);
            ReleaseValue(type_root);
            throw;
        }
        ReleaseValue(value_root);
        ReleaseValue(type_root);
        return flag_arrays.at(flag);
    }
};
EffectInputs::EffectInputs(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r)
    : state_(new State(m, d, o, r)) {}
EffectInputs::~EffectInputs() {
    delete state_;
}
ObjectResult EffectInputs::Manager() {
    if (state_->closed)
        return {};
    const auto field = state_->Get(EffectInputField::Manager);
    ObjectResult out;
    out.call = native::Call(field.call, &out.object, field.object, static_cast<void*>(nullptr));
    return out;
}
CallResult EffectInputs::ConfigureReplay(void* parameters, bool enabled) {
    if (state_->closed || !parameters)
        return {};
    state_->PrepareFields(parameters, {EffectInputField::Replay});
    state_->Scalar(parameters, EffectInputField::Replay, static_cast<std::uint8_t>(enabled));
    return {true, 0};
}
EffectInputResult EffectInputs::Configure(void* parameters, void* target, void* completion) {
    EffectInputResult out;
    if (state_->closed || !parameters || !target || !completion)
        return out;
    // Resolve BOTH before either assignment. Failure after a write still
    // belongs to the caller's unsubmitted request, not the official loader.
    const auto entity = state_->Get(EffectInputField::Target);
    const auto callback = state_->Get(EffectInputField::Completion);
    out.target = native::CallVoid(entity.call, entity.object, parameters, target, 0, static_cast<void*>(nullptr),
                                  static_cast<void*>(nullptr));
    if (out.target.returned)
        out.completion = native::CallVoid(callback.call, callback.object, parameters, completion, 0,
                                          static_cast<void*>(nullptr), static_cast<void*>(nullptr));
    return out;
}
EffectInputResult EffectInputs::ConfigureEntity(void* parameters, void* caster, void* target, void* completion,
                                                const EntityEffectOptions& options) {
    if (!target)
        return {};
    const auto captured = options;
    return ConfigureImpl(parameters, caster, target, completion, captured, nullptr);
}
EffectInputResult EffectInputs::ConfigureHitReferences(void* parameters, void* caster, void* target, void* completion) {
    auto& s = *state_;
    EffectInputResult out;
    if (s.closed || !parameters || !caster || !target || !completion)
        return out;
    const auto caster_field = s.Get(EffectInputField::Caster);
    const auto target_field = s.Get(EffectInputField::Target);
    const auto callback_field = s.Get(EffectInputField::Completion);
    out.target = s.Set(caster_field, parameters, caster);
    if (out.target.returned)
        out.target = s.Set(target_field, parameters, target);
    if (out.target.returned)
        out.completion = s.Set(callback_field, parameters, completion);
    return out;
}
EffectInputResult EffectInputs::ConfigureSpatial(void* parameters, void* caster, void* completion,
                                                 const SpatialEffectOptions& input) {
    auto spatial = input;
    const auto& r = spatial.rotation;
    const double squared = double(r.x) * r.x + double(r.y) * r.y + double(r.z) * r.z + double(r.w) * r.w;
    if (!std::isfinite(spatial.position.x) || !std::isfinite(spatial.position.y) ||
        !std::isfinite(spatial.position.z) || !std::isfinite(squared) || squared == 0)
        throw std::invalid_argument("invalid spatial effect pose");
    const auto length = std::sqrt(squared);
    spatial.rotation = {float(r.x / length), float(r.y / length), float(r.z / length), float(r.w / length)};
    EntityEffectOptions options;
    options.unique_name = spatial.unique_name;
    options.scale = spatial.scale;
    options.fade_in = spatial.fade_in;
    options.simulate_immediately = spatial.simulate_immediately;
    options.resident = spatial.resident;
    return ConfigureImpl(parameters, caster, nullptr, completion, options, &spatial);
}
EffectInputResult EffectInputs::ConfigureImpact(void* parameters, void* caster, void* target, void* completion,
                                                const ImpactEffectOptions& options) {
    auto& s = *state_;
    EffectInputResult out;
    if (s.closed || !parameters || !caster || !target || !completion)
        return out;
    const auto finite = [](Position3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
    if (!finite(options.position) || !finite(options.direction))
        throw std::invalid_argument("invalid impact pose");
    s.PrepareFields(parameters, {EffectInputField::InitialPosition, EffectInputField::InitialRotation,
                                 EffectInputField::ParamUsage});
    Rotation4 rotation{};
    if (options.align) {
        auto forward = options.direction;
        Position3 up{0, 1, 0};
        const auto method = detail::ResolveEffectLookRotation(s.methods);
        if (!native::CallVoid(method, &forward, &up, &rotation).returned)
            throw std::runtime_error("impact orientation unavailable");
    }
    const auto caster_field = s.Get(EffectInputField::Caster);
    const auto param_field = s.Get(EffectInputField::ParamEntity);
    const auto target_field = s.Get(EffectInputField::Target);
    const auto callback_field = s.Get(EffectInputField::Completion);
    const auto flags_field = s.Get(EffectInputField::Flags);
    auto* flags =
        s.FlagArray(EffectInputConventions::impact_flag); // ordinary projectile hit helper adds MonoEffectFlag 6
    out.target = s.Set(caster_field, parameters, caster);
    if (out.target.returned)
        out.target = s.Set(param_field, parameters, target);
    if (out.target.returned)
        out.target = s.Set(target_field, parameters, options.attach ? target : nullptr);
    if (out.target.returned)
        out.target = s.Set(flags_field, parameters, flags);
    if (out.target.returned) {
        using NullableVector = EffectNullableVector;
        using NullableRotation = EffectNullableRotation;
        // Same normal-hit inputs: no inferred attach point or position offset,
        // and no generic entity-relative/reset flags layered on top of them.
        s.Scalar(parameters, EffectInputField::InitialPosition, NullableVector{options.position, 1});
        s.Scalar(parameters, EffectInputField::InitialRotation, NullableRotation{rotation, 1});
        s.Scalar(parameters, EffectInputField::ParamUsage, std::int32_t{EffectInputConventions::impact_usage});
        out.completion = s.Set(callback_field, parameters, completion);
    }
    return out;
}
EffectInputResult EffectInputs::ConfigureImpl(void* parameters, void* caster, void* target, void* completion,
                                              const EntityEffectOptions& options, const SpatialEffectOptions* spatial) {
    auto& s = *state_;
    EffectInputResult out;
    if (s.closed || !parameters || !caster || !completion)
        return out;
    const auto finite = [](Position3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    if (!finite(options.position_offset) || !finite(options.rotation_offset) ||
        (options.scale && !finite(*options.scale)) || options.unique_name.find('\0') != std::string::npos ||
        options.attach_point.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid entity effect visual inputs");
    if (spatial)
        s.PrepareSpatial(parameters);
    else
        s.PrepareEntity(parameters);
    const auto caster_field = s.Get(EffectInputField::Caster);
    const auto target_field = s.Get(EffectInputField::Target);
    const auto name_field = s.Get(EffectInputField::UniqueName);
    const auto callback_field = s.Get(EffectInputField::Completion);
    const auto attach_field = s.Get(EffectInputField::AttachPoint);
    const auto flags_field = s.Get(EffectInputField::Flags);
    auto* flags = options.resident ? s.FlagArray(EffectInputConventions::resident_flag) : nullptr;
    ManagedRootId text_root = 0, attach_root = 0;
    try {
        auto* name = s.Keep(s.objects.NewStringUtf8(options.unique_name.c_str()));
        text_root = s.retained.back();
        void* attach_point = nullptr;
        if (!options.attach_point.empty()) {
            attach_point = s.Keep(s.objects.NewStringUtf8(options.attach_point.c_str()));
            attach_root = s.retained.back();
        }
        out.target = s.Set(caster_field, parameters, caster);
        if (out.target.returned)
            out.target = s.Set(target_field, parameters, target);
        if (out.target.returned)
            out.target = s.Set(name_field, parameters, name);
        // The official task copies TriggerEffect.AttachPoint to DynamicAttach.
        // The loader derives AttachTransform/AttachPointName before Reset;
        // writing those cached outputs would be overwritten on load.
        if (out.target.returned)
            out.target = s.Set(attach_field, parameters, attach_point);
        if (out.target.returned)
            out.target = s.Set(flags_field, parameters, flags);
        if (out.target.returned) {
            using NullableVector = EffectNullableVector;
            const auto relative = static_cast<std::uint8_t>(!spatial);
            const NullableVector position{options.position_offset, relative},
                rotation{options.rotation_offset, relative};
            const NullableVector scale{options.scale.value_or(Position3{}),
                                       static_cast<std::uint8_t>(options.scale.has_value())};
            s.Scalar(parameters, EffectInputField::PositionOffset, position);
            s.Scalar(parameters, EffectInputField::RotationOffset, rotation);
            s.Scalar(parameters, EffectInputField::Scale, scale);
            s.Scalar(parameters, EffectInputField::FadeIn, static_cast<std::uint8_t>(options.fade_in));
            s.Scalar(parameters, EffectInputField::SimulateImmediately,
                     static_cast<std::uint8_t>(options.simulate_immediately));
            s.Scalar(parameters, EffectInputField::Replay, std::uint8_t{0});
            // Original task default is LookAt. ParamEntity stays null: no
            // toward evaluator or corresponding completion processing implied.
            s.Scalar(parameters, EffectInputField::ParamUsage, std::int32_t{EffectInputConventions::look_at_usage});
            s.Scalar(parameters, EffectInputField::MaxPitch, EffectInputConventions::toward_max_pitch);
            if (spatial) {
                // The loader consumes InitPosition before ResetTransform uses
                // AnchorPosition. Populate both, without a target/attachment.
                const NullableVector world{spatial->position, 1};
                using NullableRotation = EffectNullableRotation;
                const NullableRotation orientation{spatial->rotation, 1};
                s.Scalar(parameters, EffectInputField::InitialPosition, world);
                s.Scalar(parameters, EffectInputField::AnchorPosition, world);
                s.Scalar(parameters, EffectInputField::InitialRotation, orientation);
                s.Scalar(parameters, EffectInputField::IgnorePrefabAttachment, std::uint8_t{1});
            }
            out.target = native::CallVoid(s.attach, parameters, options.attach_to_target);
        }
        if (out.target.returned)
            out.completion = s.Set(callback_field, parameters, completion);
    } catch (...) {
        s.ReleaseValue(attach_root);
        s.ReleaseValue(text_root);
        throw;
    }
    s.ReleaseValue(attach_root);
    s.ReleaseValue(text_root);
    return out;
}
bool EffectInputs::Close() {
    auto& s = *state_;
    s.closed = true;
    s.fields.clear();
    for (auto it = s.retained.begin(); it != s.retained.end();) {
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.retained.erase(it);
        } else
            ++it;
    }
    return s.retained.empty();
}
}
