#include "../../../adapters/presentation/EffectSchemaBindings.hpp"
#include <hksr/presentation/effects/EffectRelations.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::api {
namespace {
template <class T> Observed<T> ReadField(void* object, std::size_t offset) {
    Observed<T> result;
    if (object)
        result.read.returned = native::CopyMemoryChecked(&result.value, static_cast<unsigned char*>(object) + offset,
                                                         sizeof(result.value));
    return result;
}
ObjectResult ReadReference(void* object, std::size_t offset) {
    const auto result = ReadField<void*>(object, offset);
    return {result.read, result.value};
}
}
EffectRelations::EffectRelations(const std::vector<DeclaredField>& fields) {
    const auto layout = detail::ResolveEffectRelationsLayout(fields);
    caster_ = layout.caster;
    target_ = layout.target;
    param_entity_ = layout.param_entity;
    instance_ = layout.instance;
    id_ = layout.id;
    released_ = layout.released;
}
EffectRelationsSnapshot EffectRelations::Read(EffectReference request) const {
    EffectRelationsSnapshot out;
    out.id_before = ReadField<std::uint32_t>(request.parameters, id_);
    out.caster = ReadReference(request.parameters, caster_);
    out.target = ReadReference(request.parameters, target_);
    out.param_entity = ReadReference(request.parameters, param_entity_);
    out.instance = ReadReference(request.parameters, instance_);
    out.released = ReadField<bool>(request.parameters, released_);
    out.id_after = ReadField<std::uint32_t>(request.parameters, id_);
    out.same_generation = out.id_before.read.returned && out.id_after.read.returned &&
                          out.id_before.value == request.id && out.id_after.value == request.id;
    return out;
}
}
