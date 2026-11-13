#pragma once
#include <hksr/managed/ManagedObjects.hpp>
namespace hksr::api::detail {
inline ObjectResult ResolveAudioGlobalType(ManagedObjects& objects) {
    return objects.ManagedType("RPG.Client", "GlobalVars");
}
inline ObjectResult NewAudioManagerFieldName(ManagedObjects& objects) {
    return objects.NewStringUtf8("s_AudioManager");
}
}
