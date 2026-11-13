#include "RenderingBindings.hpp"
#include <stdexcept>

namespace hksr::api::detail {
namespace {
std::size_t Offset(const RenderingFields& fields, const char* ns, const char* owner, const char* name,
                   const char* type) {
    std::optional<std::size_t> result;
    for (const auto& field : fields(ns, owner, false))
        if (field.name == name) {
            if (result || field.type != type || !field.flags_observed || (field.flags & 16))
                throw std::runtime_error("actor rendering field differs");
            result = field.offset;
        }
    if (!result)
        throw std::runtime_error(std::string("actor rendering field missing: ") + name);
    return *result;
}
template <class F> void Bind(std::string& error, F&& bind) {
    try {
        bind();
    } catch (const std::exception& failure) {
        error = failure.what();
    }
}
}
std::size_t ResolveRenderingRendererCacheOffset(const RenderingFields& read) {
    std::optional<std::size_t> result;
    for (const auto& field : read("RPG.Client", "BaseShaderPropertyTransition", false))
        if (field.name == "OIDAGBPPNBJ") {
            if (result || field.owner_namespace != "RPG.Client" || field.owner_name != "BaseShaderPropertyTransition" ||
                field.type != "DEBEAKKOHLN" || !field.flags_observed || (field.flags & 16))
                throw std::runtime_error("actor renderer cache declaration differs");
            result = field.offset;
        }
    if (!result)
        throw std::runtime_error("actor renderer cache base field missing");
    return *result;
}
RenderingBindings ResolveRenderingBindings(MethodResolver& m, const RenderingFields& fields, const RenderingType& type,
                                           const RenderingOutlineList& bind_list,
                                           const RenderingOutlineList& write_list) {
    RenderingBindings s;
    Bind(s.model_error, [&] { s.model_type = type("RPG.GameCore", "CharacterModelComponent"); });
    Bind(s.renderer_error, [&] {
        s.renderer_type = type("UnityEngine", "Renderer");
        s.alive = m.Resolve("UnityEngine", "Object", "op_Implicit", "System.Boolean", {"UnityEngine.Object"},
                            MethodKind::Static);
    });
    Bind(s.mask_read_error,
         [&] { s.mask_read = m.Resolve("UnityEngine", "Renderer", "get_renderingLayerMask", "System.UInt32", {}); });
    Bind(s.mask_write_error, [&] {
        s.mask_write = m.Resolve("UnityEngine", "Renderer", "set_renderingLayerMask", "System.Void", {"System.UInt32"});
    });
    Bind(s.attachment_error, [&] {
        s.mapping_type = type("RPG.Client", "AttachPointMapping");
        s.root_component =
            m.Resolve("UnityEngine", "GameObject", "GetComponent", "UnityEngine.Component", {"System.Type"});
        s.find_attach = m.Resolve("RPG.Client", "AttachPointMapping", "FindAttachPoint", "UnityEngine.Transform",
                                  {"System.String", "System.Boolean", "System.String"});
        s.attachment_game_object =
            m.Resolve("UnityEngine", "Component", "get_gameObject", "UnityEngine.GameObject", {});
    });
    Bind(s.outline_error, [&] {
        s.effect_type = type("RPG.Client", "CharacterEffect");
        s.component = m.Resolve("UnityEngine", "Component", "GetComponent", "UnityEngine.Component", {"System.Type"});
        s.component_transform = m.Resolve("UnityEngine", "Component", "get_transform", "UnityEngine.Transform", {});
        s.parent = m.Resolve("UnityEngine", "Transform", "get_parent", "UnityEngine.Transform", {});
        s.outline_game_object = m.Resolve("UnityEngine", "Component", "get_gameObject", "UnityEngine.GameObject", {});
        s.outline_add = Offset(fields, "RPG.Client", "CharacterEffect", "NIOHCEKBNFF",
                               "System.Collections.Generic.List<UnityEngine.Renderer>");
        s.outline_remove = Offset(fields, "RPG.Client", "CharacterEffect", "FNEIHJDCMMM",
                                  "System.Collections.Generic.List<UnityEngine.Renderer>");
        bind_list(s);
    });
    Bind(s.outline_write_error, [&] {
        if (!s.outline_error.empty())
            throw std::runtime_error(s.outline_error);
        s.outline_set = m.Resolve("RPG.Client", "CharacterEffect", "SetExtraTargetRenders", "System.Void",
                                  {"System.Boolean", "System.Collections.Generic.IReadOnlyList<UnityEngine.Renderer>",
                                   "System.Collections.Generic.IReadOnlyList<UnityEngine.Renderer>"});
        if (write_list)
            write_list(s);
    });
    Bind(s.named_error, [&] {
        s.shader_property = m.Resolve("", "NPCEHHGJBNH", "NMGGBILIIMP", "RPG.Client.CharacterShaderPropertyTransition",
                                      {"RPG.GameCore.GameEntity"}, MethodKind::Static);
        s.find_renderer = m.Resolve("", "DEBEAKKOHLN", "OGMPOCEMKIH", "UnityEngine.Renderer", {"System.String"});
        s.renderer_cache = ResolveRenderingRendererCacheOffset(fields);
    });
    Bind(s.visibility_error, [&] {
        s.visible_type = type("RPG.GameCore", "CharacterVisibleComponent");
        s.visibility_set = m.Resolve("RPG.GameCore", "CharacterVisibleComponent", "SetEntityVisibleParam",
                                     "System.Void", {"System.Boolean", "ANACIPLIMDE&"});
        // This unboxed value is passed by reference; these four offsets are its actual ABI.
        if (Offset(fields, "", "ANACIPLIMDE", "KDOLHICFKLL", "System.String") != 16 ||
            Offset(fields, "", "ANACIPLIMDE", "LPEFHLMHLPN", "System.Boolean") != 24 ||
            Offset(fields, "", "ANACIPLIMDE", "ONMFCPIJGNP", "System.Boolean") != 25 ||
            Offset(fields, "", "ANACIPLIMDE", "IEMGPGLKIIG", "System.Boolean") != 26)
            throw std::runtime_error("visibility value ABI differs");
    });
    return s;
}
RenderingBindings ResolveRenderingBindings(RuntimeMetadata& metadata, const RenderingType& type) {
    return ResolveRenderingBindings(
        metadata,
        [&](const char* ns, const char* owner, bool parents) { return metadata.ReadFields(ns, owner, parents); }, type,
        [&](RenderingBindings& s) {
            const auto fields = metadata.ObserveFields(metadata.FindType("RPG.Client", "CharacterEffect"));
            if (!fields.iteration_error.empty())
                throw std::runtime_error(fields.iteration_error);
            for (const auto& row : fields.fields)
                if (row.declaration.name == "NIOHCEKBNFF") {
                    if (!row.error.empty())
                        throw std::runtime_error(row.error);
                    s.list_type = metadata.FieldType(row.field);
                }
            if (!s.list_type)
                throw std::runtime_error("outline closed list type missing");
            s.list.emplace(ResolveReferenceListHierarchy(metadata, s.list_type, "UnityEngine.Renderer"));
        },
        [&](RenderingBindings& s) {
            s.list_ctor = metadata.Resolve(s.list_type, ".ctor", "System.Void", {});
            s.list_add = metadata.Resolve(s.list_type, "Add", "System.Void", {"UnityEngine.Renderer"});
        });
}

}
