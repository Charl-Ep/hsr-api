#include <hksr/presentation/PresentationBindings.hpp>
#include <exception>

namespace hksr::sdk_v1 {
PresentationBindings ResolvePresentationDeclarations(MethodResolver& metadata,const DeclaredFieldLookup& read_fields) {
    PresentationBindings out;
    auto bind=[&](const char* name,auto& target,auto get) {
        try {target=get();}
        catch(const std::exception& error) {out.errors.push_back(std::string(name)+": "+error.what());}
    };
    auto fields=[&](const char* ns,const char* type) {
        std::vector<DeclaredField> result;
        bind(type,result,[&]{return read_fields(ns,type);});return result;
    };
    const auto model_fields=fields("RPG.GameCore","CharacterModelComponent");
    const auto event_fields=fields("","LGBHONLHNPN");
    const auto loader_fields=fields("","NLMPFKPGCNA");
    bind("NPC factory",out.factory,[&]{return ResolveMapNpcFactory(metadata);});
    bind("references",out.references,[&]{return ResolveReferenceMethods(metadata);});
    if(!out.references.retain.code) // plain NPC/identity does not need asset leases
        bind("GC references",out.references,[&]{return ResolveGcMethods(metadata);});
    out.entity=ResolveEntityAccessMethods(metadata);
    out.model=ResolveModelAccessMethods(metadata,model_fields);
    bind("renderer",out.renderer,[&]{return ResolveRendererMethods(metadata);});
    bind("Unity lifetime",out.lifetime,[&]{return ResolveUnityLifetimeMethods(metadata);});
    out.admission=ResolveArtAdmissionMethods(metadata,model_fields);
    out.visual=ResolveVisualObjectMethods(metadata);
    out.config=ResolveAdventureConfigMethods(metadata,fields("RPG.GameCore","AdventureCharacterConfig"));
    out.adventure_data=ResolveAdventureDataField(fields("RPG.GameCore","AdventureCharacterDataComponent"));
    out.events=ResolveAnimationEventMethods(metadata,event_fields);
    out.profile_inputs=ResolveProfileInputMethods(metadata);
    out.profile_events=ResolveProfileEventFields(fields("","HHECHLOIEJO"));
    bind("model profile",out.profile,[&]{return ResolveModelMethods(metadata);});
    out.animation=ResolveAnimationControlMethods(metadata);
    ResolveBlinkControl(out.animation,metadata,fields("","PEPNFLNCDHG"));
    ResolveHitMotionReads(out.animation,metadata,fields("","BGOBCKLPEMK"));
    out.model_loader=ResolveModelControllerLoader(model_fields);
    out.loader_model=ResolveControllerLoaderModel(loader_fields);
    out.loader_manager=ResolveControllerLoaderManager(loader_fields);
    out.scene=ResolveSceneObjectMethods(metadata);
    out.components=ResolveComponentConfigMethods(metadata);
    out.arrays=ResolveArrayMethods(metadata);
    bind("hierarchy",out.hierarchy,[&]{return ResolveHierarchyMethods(metadata);});
    out.mesh=ResolveMeshMethods(metadata);
    out.mesh_templates=ResolveMeshTemplateMethods(metadata);
    out.texture=ResolveTextureMethods(metadata);
    out.material=ResolveMaterialMethods(metadata);
    out.shader=ResolveShaderMethods(metadata);
    out.world_map=ResolveWorldMapMethods(metadata);
    out.placement=ResolveWorldPlacementMethods(metadata);
    out.actor_control=ResolveActorControlMethods(metadata);
    out.hit_geometry=ResolveHitGeometryMethods(metadata,fields("RPG.GameCore","TransformComponent"));
    out.hit_direction=ResolveHitDirectionMethods(metadata);
    ResolveHitDirectionModel(out.hit_direction,model_fields);
    // Keep per-operation partial results and one readable startup summary.
    auto errors=[&](const auto& group){out.errors.insert(out.errors.end(),group.binding_errors.begin(),group.binding_errors.end());};
    errors(out.entity);errors(out.model);errors(out.admission);errors(out.visual);
    errors(out.config);errors(out.adventure_data);errors(out.events);errors(out.event_list);
    errors(out.profile_inputs);errors(out.profile_events);errors(out.animation);errors(out.scene);
    errors(out.components);errors(out.arrays);errors(out.mesh);errors(out.mesh_templates);
    errors(out.texture);errors(out.material);errors(out.shader);errors(out.world_map);
    errors(out.placement);
    errors(out.actor_control);
    errors(out.hit_geometry);errors(out.hit_direction);
    for(const auto* link:{&out.model_loader,&out.loader_model,&out.loader_manager})
        if(!link->available) out.errors.push_back(link->error);
    return out;
}
namespace {
PresentationBindings ResolveBindings(RuntimeMetadata& metadata) {
    auto out=ResolvePresentationDeclarations(metadata,[&](const char* ns,const char* type){return metadata.ReadFields(ns,type);});
    try {out.event_list=ResolveReturnedReferenceList(metadata,out.events.records,"LDMKDJJLOIL");}
    catch(const std::exception& e) {out.errors.push_back(std::string("event list: ")+e.what());}
    out.errors.insert(out.errors.end(),out.event_list.binding_errors.begin(),out.event_list.binding_errors.end());
    try {out.skinning=ResolveMeshSkinning(metadata.Version(),metadata);}
    catch(const std::exception& e) {out.errors.push_back(std::string("skinning: ")+e.what());}
    return out;
}
}
PresentationBindings ResolvePresentationBindings(RuntimeMetadata& metadata) {
    return ResolveBindings(metadata);
}
}
