#pragma once
#include <hksr/model/ModelRelease.hpp>

namespace hksr::api::detail {
template <class Body> struct ModelReleaseThunk {
    static void __fastcall ArtUnload(CharacterModelComponent* model, bool option, void* metadata) {
        Body::Before(model, ModelReleaseBoundary::ArtUnload, option);
        reinterpret_cast<void(__fastcall*)(CharacterModelComponent*, bool, void*)>(Body::Original(0))(model, option,
                                                                                                      metadata);
    }
    static void __fastcall RootRelease(CharacterModelComponent* model, void* metadata) {
        Body::Before(model, ModelReleaseBoundary::RootRelease, false);
        reinterpret_cast<void(__fastcall*)(CharacterModelComponent*, void*)>(Body::Original(1))(model, metadata);
    }
    static void* ArtAddress() {
        return reinterpret_cast<void*>(&ArtUnload);
    }
    static void* RootAddress() {
        return reinterpret_cast<void*>(&RootRelease);
    }
};
}
