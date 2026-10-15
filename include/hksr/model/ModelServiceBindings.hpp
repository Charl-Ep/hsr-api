#pragma once
#include "ArtAdmission.hpp"
#include "../entity/EntityAccess.hpp"
#include "../unity/RendererAccess.hpp"
#include "../unity/OwnedUnityObjects.hpp"

namespace hksr::sdk_v1 {
// Explicit low-level composition for custom providers and tests.
struct ModelServiceBindings {
    ReferenceMethods references;
    UnityLifetimeMethods lifetime;
    RendererMethods renderers;
    EntityAccessMethods entities;
    ModelAccess models{ModelAccessMethods{}};
    ArtAdmission admission{ArtAdmissionMethods{}};
};
}
