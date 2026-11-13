#include "ProjectileWrapperBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <cstring>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
void* ConstructorEntry(Method method) {
    // Unity's managed .ctor(string) is a RIP-relative import thunk in the current client.
    // Native game bodies call that same slot directly. Resolve the declaration,
    // then the checked forwarding instruction, never a stored image RVA.
    unsigned char bytes[9]{};
    if (!method.code || !native::CopyMemoryChecked(bytes, method.code, sizeof(bytes)))
        throw std::runtime_error("Unity constructor entry unreadable");
    if (bytes[0] == 0x48 && bytes[1] == 0x8b && bytes[2] == 0x05 && bytes[7] == 0xff && bytes[8] == 0xe0) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 3, sizeof(displacement));
        void* target = nullptr;
        if (!native::CopyMemoryChecked(&target, static_cast<char*>(method.code) + 7 + displacement, sizeof(target)) ||
            !target)
            throw std::runtime_error("Unity constructor forwarding target absent");
        return target;
    }
    return method.code; // direct managed implementations / native-table fixtures
}
}
ProjectileWrapperBindings ResolveProjectileWrapperBindings(MethodResolver& resolver) {
    const auto factory =
        resolver.Resolve("", "NAOKFLDEDBJ", "DPMCKGFPGKO", "UnityEngine.GameObject", {"RPG.Client.MonoEffect"});
    const auto ctor =
        ConstructorEntry(resolver.Resolve("UnityEngine", "GameObject", ".ctor", "System.Void", {"System.String"}));
    const auto active = resolver.Resolve("UnityEngine", "GameObject", "SetActive", "System.Void", {"System.Boolean"});
    return {factory, ctor, active};
}
}
