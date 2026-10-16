#include <hksr/model/ModelProfile.hpp>
#include "../../src/metadata/ResolveBatch.hpp"

namespace hksr::sdk_v1 {
Method ResolveModelPending(MethodResolver& resolver) {
    return resolver.Resolve("","HLCPCNFMECN","LEHKGLNGAGF","System.Boolean",{});
}
ModelMethods ResolveModelMethods(MethodResolver& resolver){
    detail::ResolveBatch batch(resolver);
    ModelMethods result{
        batch.Resolve("","HLCPCNFMECN","FHLALJENMHB","System.Void",
            {"RPG.GameCore.GameEntity","MOIKMFOGHAO","System.Boolean","System.Action"}),
        batch.Resolve("","HLCPCNFMECN","HADOHLNHHGB","System.Void",{}),
        batch.Resolve("RPG.GameCore","CharacterModelComponent","ChangeModel","System.Boolean",
            {"System.String","UnityEngine.GameObject"}),
        batch.Resolve("","LGBHONLHNPN","FOGNIIEDKCH","System.Void",{"System.String[]","System.String[]"}),
        batch.Resolve("RPG.GameCore","CharacterModelComponent","CanChangeModel","System.Boolean",
            {"System.String","System.String&"}),
        batch.Resolve("RPG.GameCore","CharacterModelComponent","_ChangeModelImpl","System.Void",
            {"System.String","System.String","UnityEngine.GameObject"})
    };
    batch.RequireComplete("model profile");return result;
}
}
