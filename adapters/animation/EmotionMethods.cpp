#include <hksr/animation/ActorEmotion.hpp>

namespace hksr::api {
ActorEmotionMethods ResolveActorEmotionMethods(MethodResolver& m) {
    ActorEmotionMethods out;
    out.graph = m.Resolve("", "PEPNFLNCDHG", "AGEEOFENFMP", "RPG.Client.MonoEmoPlayableGraph", {});
    out.config_path = m.Resolve("RPG.GameCore", "CharacterDataComponent", "get_GraphEmotionAsset", "System.String", {});
    out.apply_config =
        m.Resolve("", "PEPNFLNCDHG", "BDIIADDGLBO", "System.Void", {"RPG.Client.EmoPlayableGraphConfig"});
    out.select_mode = m.Resolve("", "PEPNFLNCDHG", "GNKBMOIOFFC", "System.Void", {"System.Boolean"});
    out.auto_blink =
        m.Resolve("RPG.Client", "MonoEmoPlayableGraph", "SetEnableAutoBlinkState", "System.Void", {"System.Boolean"});
    out.eye_blink = m.Resolve("RPG.Client", "MonoEmoPlayableGraph", "SetEnableAutoBlickWithEyeState", "System.Void",
                              {"System.Boolean"});
    out.always_evaluate =
        m.Resolve("RPG.Client", "MonoEmoPlayableGraph", "set_AlwaysEvaluate", "System.Void", {"System.Boolean"});
    out.built_graph = m.Resolve("RPG.Client", "MonoEmoPlayableGraph", "get_emotionPlayableGraph", "BKLJMPGNFNB", {});
    return out;
}
}
