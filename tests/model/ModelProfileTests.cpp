#include <hksr/model/ModelProfile.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/entity/EntityFactory.hpp>
#include <hksr/managed/References.hpp>
#include <Windows.h>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>

using namespace hksr::api;
void TestNativeMetadataReader();
void TestCharacterOutfit();
void TestEventRegistrationLease();
void TestNpcEventSupplement();
void TestNpcCreationSessions();
void TestControllerBinding();
void TestOwnedEffectRequest();
void TestModelBaselineCapture();
void TestModelTransition();
void TestActorControlSession();
void TestLookSupport();
void TestActionRelations();
void TestAnimationResources();
void TestAnimationOverrides();
void TestEntityLifecycle();
void TestEntityProvenance();
void TestConfigProjection();
void TestLifecycleHooks();
void TestAssetLeases();
void TestWorldFrames();
void TestManagedInputs();
void TestNpcInputs();
void TestEntityAccess();
void TestUnityLifetime();
void TestUnityAssets(const char* fixture);
void TestMeshSkinning();
void TestMeshWriterLocator();
static int object,entity,path,config,events,layers,metadata;
static unsigned calls=0,cancels=0,restores=0;
static int roots=0,asset_leases=0;
static void Check(bool ok){if(!ok)throw std::runtime_error("SDK call boundary mismatch");}
static void __fastcall Begin(void* op,void* target,ModelInputs input,bool async,void* callback,void* info){
    Check(op==&object && target==&entity && !async && !callback && info==&metadata);
    Check(input.model_path==&path && input.adventure_config==&config && input.event_paths==&events && input.excluded_layers==&layers);++calls;
}
static void __fastcall Cancel(void* op,void* info){Check(op==&object && info==&metadata);++cancels;}
static bool __fastcall Change(void* model,void* p,void* prepared,void* info){Check(model==&object && p==&path && !prepared && info==&metadata);return false;}
static bool __fastcall CanChange(void* model,void* p,void** resolved,void* info){Check(model==&object && p==&path && info==&metadata);*resolved=&path;return true;}
static void __fastcall Events(void* component,void* p,void* excluded,void* info){Check(component==&object && p==&events && excluded==&layers && info==&metadata);}
static void __fastcall Restore(void* model,void* p,void* c,void* root,void* info){Check(model==&object && p==&path && c==&config && !root && info==&metadata);++restores;}
static void __fastcall Fails(void*,void*){RaiseException(0xE0424242,0,0,nullptr);}
static int __fastcall Retain(void* target,int handle,int kind,void* info){Check(target==&object && handle==0 && kind==2 && info==&metadata);return ++roots;}
static void* __fastcall Target(int handle,void* info){Check(handle>0 && info==&metadata);return &object;}
static void __fastcall Free(int handle,void* info){Check(handle>0 && info==&metadata);--roots;}
static void* __fastcall Load(void* p,void* type,bool option,void* info){Check(p==&path && type==&config && !option && info==&metadata);++asset_leases;return &object;}
static void __fastcall Drop(void* target,void* info){Check(target==&object && info==&metadata);--asset_leases;}
static GameEntity* __fastcall CreateMap(EntityManager* manager,MapNpcDefinition* definition,NpcDataRow* row,EntityCallback* callback,bool option,void* info){
    Check(manager==reinterpret_cast<EntityManager*>(&object) && definition==reinterpret_cast<MapNpcDefinition*>(&path));
    Check(row==reinterpret_cast<NpcDataRow*>(&config) && !callback && option && info==&metadata);
    return reinterpret_cast<GameEntity*>(&entity);
}
struct Resolver final:MethodResolver {
    std::vector<std::string> names;
    Method Resolve(const char*,const char*,const char* name,const char*,std::initializer_list<const char*> params,MethodKind kind) override {
        names.emplace_back(name);
        const std::string n=name;
        if(n=="GetTargetHandle"){Check(kind==MethodKind::Static && params.size()==3);return {reinterpret_cast<void*>(&Retain),&metadata};}
        if(n=="GetTarget")return {reinterpret_cast<void*>(&Target),&metadata};
        if(n=="FreeHandle")return {reinterpret_cast<void*>(&Free),&metadata};
        if(n=="SyncLoadAsset"){Check(kind==MethodKind::Static && params.size()==3);return {reinterpret_cast<void*>(&Load),&metadata};}
        if(n=="ReleaseAsset")return {reinterpret_cast<void*>(&Drop),&metadata};
        if(n=="ClientCreateMapNPC"){Check(kind==MethodKind::Static && params.size()==5);return {reinterpret_cast<void*>(&CreateMap),&metadata};}
        Check(kind==MethodKind::Instance);
        if(n=="FHLALJENMHB"){Check(params.size()==4);return {reinterpret_cast<void*>(&Begin),&metadata};}
        if(n=="HADOHLNHHGB")return {reinterpret_cast<void*>(&Cancel),&metadata};
        if(n=="ChangeModel")return {reinterpret_cast<void*>(&Change),&metadata};
        if(n=="FOGNIIEDKCH")return {reinterpret_cast<void*>(&Events),&metadata};
        if(n=="CanChangeModel")return {reinterpret_cast<void*>(&CanChange),&metadata};
        if(n=="_ChangeModelImpl"){Check(params.size()==3);return {reinterpret_cast<void*>(&Restore),&metadata};}
        throw std::runtime_error("unexpected method");
    }
};
struct MissingResolver final:MethodResolver {
    unsigned attempts=0;
    Method Resolve(const char*,const char*,const char*,const char*,std::initializer_list<const char*>,MethodKind) override {
        ++attempts;throw std::runtime_error("missing in supplied declarations");
    }
};
void TestArtAdmission();
void TestAnimationEvents();
void TestAnimationExitConfig();
void TestAnimationControl();
void TestOfficialCatalog();
void TestActionConfig();
void TestWorldMap();
void TestRuntimeThread();
void TestRuntimeHost();
void TestAnimatorCatalog();
void TestEffectCompletion();
void TestEffectAccess();
void TestEffectControl();
void TestEffectActions();
void TestEffectRelations();
void TestEffectQueries();
void TestEffectInputs();
void TestEventValueInputs();
void TestManagedConfigProjection();
void TestAnimationConfigSchema();
void TestTimelineAsset();
void TestTimelineOwnership();
void TestTimelineRegistry();
void TestTimelineGraph();
void TestMetadataLocator(const char* optional_image);
void RunTests(int argc, char** argv){
    TestEventRegistrationLease();
    TestNpcCreationSessions();
    TestNpcEventSupplement();
    TestControllerBinding();
    TestOwnedEffectRequest();
    TestModelBaselineCapture();
    TestModelTransition();
    TestActorControlSession();
    TestLookSupport();
    TestActionRelations();
    TestAnimationResources();
    TestAnimationOverrides();
    TestTimelineRegistry();
    TestTimelineGraph();
    TestEffectCompletion();
    TestEffectAccess();
    TestEffectControl();
    TestEffectActions();
    TestEffectRelations();
    TestEffectQueries();
    TestEffectInputs();
    TestCharacterOutfit();
    TestEventValueInputs();
    TestManagedConfigProjection();
    TestAnimationConfigSchema();
    TestTimelineAsset();
    TestTimelineOwnership();
    TestAnimatorCatalog();
    TestRuntimeHost();
    TestRuntimeThread();
    TestWorldMap();
    TestActionConfig();
    Check(argc==2 || argc==3);
    TestMetadataLocator(argc==3?argv[2]:nullptr);
    TestMeshSkinning();
    TestMeshWriterLocator();
    TestUnityAssets(argv[1]);
    TestNativeMetadataReader();
    TestEntityLifecycle();
    TestEntityProvenance();
    TestConfigProjection();
    TestLifecycleHooks();
    TestAssetLeases();
    TestWorldFrames();
    TestManagedInputs();
    TestNpcInputs();
    TestEntityAccess();
    TestUnityLifetime();
    TestArtAdmission();
    TestAnimationEvents();
    TestAnimationExitConfig();
    TestAnimationControl();
    TestOfficialCatalog();
    MissingResolver failures;bool group_failed=false;
    try{ResolveModelMethods(failures);}catch(const std::exception& error){
        group_failed=std::string(error.what()).find("_ChangeModelImpl")!=std::string::npos;
    }
    Check(group_failed && failures.attempts==6);
    DeclaredType type{"Game","Model",{{"Change","System.Boolean",{"System.String"},false,{reinterpret_cast<void*>(&Change),&metadata}}}};
    MetadataResolver actual({type});Check(actual.Resolve("Game","Model","Change","System.Boolean",{"System.String"}).metadata==&metadata);
    auto missing=[&](MetadataResolver& registry){try{registry.Resolve("Game","Model","Change","System.Boolean",{"System.String"});return false;}catch(const std::exception&){return true;}};
    MetadataResolver absent({});Check(missing(absent));
    MetadataResolver duplicate({type,type});Check(missing(duplicate));
    type.methods[0].is_static=true;MetadataResolver wrong_kind({type});Check(missing(wrong_kind));
    Check(wrong_kind.Resolve("Game","Model","Change","System.Boolean",{"System.String"},MethodKind::Static).metadata==&metadata);
    // Metadata lookup does not execute a method or require a probe record.
    Check(calls==0 && cancels==0 && restores==0);
    Resolver resolver;const auto methods=ResolveModelMethods(resolver);Check(resolver.names.size()==6);
    ModelProfile api(methods);
    Check(api.Begin(&object,&entity,{&path,&config,&events,&layers},false,nullptr).returned && calls==1);
    Check(api.Cancel(&object).returned && cancels==1 && restores==0);
    const auto change=api.Change(&object,&path,nullptr);Check(change.call.returned && !change.changed && !change.call.exception);
    void* resolved=nullptr;Check(api.CanChange(&object,&path,&resolved).changed && resolved==&path);
    Check(api.ReplaceEvents(&object,&events,&layers).returned);
    Check(api.Restore(&object,&path,&config).returned && restores==1);
    auto failed=methods;failed.cancel={reinterpret_cast<void*>(&Fails),&metadata};
    const auto result=ModelProfile(failed).Cancel(&object);Check(!result.returned && result.exception==0xE0424242);
    Check(!ModelProfile({}).Cancel(&object).returned);
    const EntityFactory factory(ResolveMapNpcFactory(resolver));
    auto created=factory.CreateMapNpc(reinterpret_cast<EntityManager*>(&object),reinterpret_cast<MapNpcDefinition*>(&path),reinterpret_cast<NpcDataRow*>(&config),nullptr,true);
    Check(created.call.returned && created.entity==reinterpret_cast<GameEntity*>(&entity));
    const References references(ResolveReferenceMethods(resolver));
    auto first=references.LoadAsset(&path,&config,false),second=references.LoadAsset(&path,&config,false);
    Check(first.call.returned && second.call.returned && first.object==second.object && asset_leases==2);
    auto root=references.Retain(first.object);Check(root.call.returned && root.handle && references.Target(root.handle).object==first.object);
    Check(references.ReleaseAsset(first.object).returned && asset_leases==1);
    Check(references.ReleaseAsset(second.object).returned && asset_leases==0);
    Check(references.Free(root.handle).returned && roots==0);
    std::cout<<"SDK DLL invocation tests passed; no target-game acceptance implied.\n";
}
int main(int argc,char** argv) {
    try {RunTests(argc,argv);return 0;}
    catch(const std::exception& error){std::cerr<<"SDK test failure: "<<error.what()<<'\n';return 1;}
}
