#include <hksr/runtime/RuntimeHost.hpp>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationResources.hpp>
#include <hksr/model/ModelServices.hpp>
#include <hksr/model/ProfileInputs.hpp>
#include <hksr/entity/EntityAccess.hpp>
#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/animation/AnimationResources.hpp>
#include <hksr/unity/UnityAssetAccess.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <memory>

namespace {
namespace api=hksr::api;
// Built against the consumer package, not against provider binding tables.
struct Capabilities {
    std::shared_ptr<const api::ModelServices> models;
    api::MeshAccess mesh;
    api::ManagedArrays arrays;
    api::EntityAccess entities;
    api::PresentationContext::AnimationFactory animations;
    explicit Capabilities(const api::PresentationContext& context)
        :models(context.Models()),mesh(context.Resources().Mesh()),
         arrays(context.Resources().Arrays()),entities(models->Entities()),animations(context.Animations()){}
};
}

extern "C" __declspec(dllexport) bool ConsumerSmoke() {
    const auto status=hksr::api::ReadRuntimeHostStatus();
    return status.phase==hksr::api::RuntimeHostPhase::Stopped && !status.resident;
}

extern "C" __declspec(dllexport) void* ConsumerOpen(const api::PresentationContext* context) noexcept {
    try{return new Capabilities(*context);}catch(...){return nullptr;}
}
extern "C" __declspec(dllexport) bool ConsumerExercise(void* handle,api::EntityToken token,void* mesh,api::CharacterModelComponent* model) noexcept {
    try {
        auto& c=*static_cast<Capabilities*>(handle);
        auto copy=c.mesh;auto assigned=copy;assigned=c.mesh;
        // Moving these immutable handles intentionally leaves a usable source.
        auto moved=std::move(copy);
        for(const auto* access:{&copy,&assigned,&moved}) {
            const auto counts=access->Counts(mesh);
            if(!counts.vertices.read.returned || counts.vertices.value!=42)return false;
        }
        const auto length=c.arrays.Length(mesh);
        if(!length.read.returned || length.value!=3)return false;
        const auto entity=c.entities.Read(token.entity);
        if(!entity.loaded.read.returned || !entity.loaded.value)return false;
        auto roots=c.models->CreateRoots();const auto id=roots->Keep(mesh);
        const bool retained=roots->Status(id).ready && roots->Target(id).object==mesh;
        const bool released=roots->Release(id).complete;roots->Forget(id);
        if(!retained || !released)return false;
        auto first=c.animations(token),second=c.animations(token);
        first->Resources().RetainOwner();second->Resources().RetainOwner();
        const auto read=c.models->Models().Read(model);
        const bool model_ok=read.root.read.returned && read.root.value==mesh &&
            read.animator.read.returned && read.animator.value==mesh;
        const auto controller=first->Control().Controller(read.animator.value);
        const auto initialized=second->Inputs().AnimatorInitialized(read.animator.value);
        const bool first_closed=first->Close();
        const bool second_closed=second->Close();
        return model_ok && controller.call.returned && controller.object==mesh &&
            initialized.read.returned && initialized.value && first_closed && second_closed;
    }catch(...){return false;}
}
extern "C" __declspec(dllexport) void ConsumerClose(void* handle) noexcept {
    delete static_cast<Capabilities*>(handle);
}
