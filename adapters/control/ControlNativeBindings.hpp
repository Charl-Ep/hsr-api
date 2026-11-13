#pragma once
#include <hksr/control/ActorControl.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
namespace hksr::api::detail {
enum class ControlType { AdventureController, Model, GazeDriver, LookIk, AlternateLookIk, SceneObject, Transform };
ObjectResult ResolveControlType(ManagedObjects&, ControlType);
ObjectResult ControlHeadName(ManagedObjects&);
ObjectResult AllocateControlPoint(ManagedObjects&);
ActorControlMethods BindControlMono(const ActorControlMethods&, ObjectMetadata&, void*);
}
