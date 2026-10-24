#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/presentation/PresentationResources.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/animation/ActorAnimationSession.hpp>
#include <hksr/animation/AnimationResources.hpp>
#include <hksr/model/ModelServices.hpp>
#include <hksr/control/LookSupport.hpp>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>

namespace {
using namespace hksr::api;
static_assert(!std::is_copy_constructible_v<PresentationResources>);
static_assert(!std::is_copy_constructible_v<ActorAnimationSession>);
static_assert(!std::is_copy_constructible_v<ModelServices>);
int info,entity_data,world_data,other_world,model_type,style_type,model,style,animator,controller,mesh;
int events_type,data_type,receiver_a,receiver_b;
struct EventComponent {int padding=0;void* consumer=&receiver_a;} event_component;
auto* entity=reinterpret_cast<GameEntity*>(&entity_data);
auto* world=reinterpret_cast<GameWorld*>(&world_data);
EntityToken token;
std::map<int,void*> roots;
int next_root=0,types=0,components=0,initialized_reads=0,controller_reads=0,mesh_reads=0;
bool changing=false,initialized=true,loaded=true,missing_controller=false;
std::function<void()> component_callback,initialized_callback,type_callback;
void Check(bool value){if(!value)throw std::runtime_error("presentation read assertion failed");}
template<class T>Method M(T fn){return {reinterpret_cast<void*>(fn),&info};}
int __fastcall Keep(void* value,int,int,void*){roots[++next_root]=value;return next_root;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Release(int id,void*){Check(roots.erase(id)==1);}
GameWorld* __fastcall World(void* input,void*){Check(input==entity);return world;}
bool __fastcall Loaded(void* input,void*){Check(input==entity);return loaded;}
void* __fastcall Component(void* input,void* type,void*) {
    Check(input==entity);++components;if(component_callback)component_callback();
    if(type==&model_type)return &model;
    if(type==&events_type)return &event_component;
    Check(type==&style_type);return &style;
}
void* __fastcall Animator(void* input,void*){Check(input==&model);return &animator;}
bool __fastcall Changing(void* input,void*){Check(input==&model);return changing;}
bool __fastcall Initialized(void* input,void*) {
    Check(input==&animator);++initialized_reads;if(initialized_callback)initialized_callback();return initialized;
}
void* __fastcall Controller(void* input,void*){Check(input==&animator);++controller_reads;return missing_controller?nullptr:&controller;}
int __fastcall Vertices(void* input,void*){Check(input==&mesh);++mesh_reads;return 42;}
bool native_alive=true;
int destroys=0,loads=0,releases=0;
bool __fastcall NativeAlive(void* input,void*){Check(input==&mesh);return native_alive;}
void __fastcall Destroy(void* input,void*){Check(input==&mesh);++destroys;native_alive=false;}
void* __fastcall Load(void* path,void* type,bool option,void*) {
    Check(path==&style && type==&model_type && !option);++loads;return &mesh;
}
void __fastcall Unload(void* input,void*){Check(input==&mesh);++releases;}
int phase,group,map,transform,world_reads=0;
void* __fastcall Group(void* input,void*){Check(input==&phase);++world_reads;return &group;}
void* __fastcall Map(void* input,void*){Check(input==&group);return &map;}
std::uint32_t __fastcall MapKey(void* input,void*){Check(input==&map);return 23;}
void* __fastcall Player(void* input,void*){Check(input==&phase);++world_reads;return entity;}
void* __fastcall GameObject(void* input,void*){Check(input==entity);return &model;}
void* __fastcall Transform(void* input,void*){Check(input==&model);return &transform;}
void __fastcall Position(void* input,Position3* output,void*){Check(input==&transform);*output={1,2,3};}
void __fastcall Rotation(void* input,Rotation4* output,void*){Check(input==&transform);*output={0,0,0,1};}
struct Objects final:ManagedObjects {
    ObjectResult ManagedType(const char* ns,const char* name) override {
        if(std::string(name)=="LGBHONLHNPN"){Check(std::string(ns).empty());return {{true,0},&events_type};}
        if(std::string(name)=="AdventureCharacterDataComponent")return {{true,0},&data_type};
        Check(std::string(ns)=="RPG.GameCore");++types;if(type_callback)type_callback();
        if(std::string(name)=="CharacterModelComponent")return {{true,0},&model_type};
        Check(std::string(name)=="FreeStyleComponent");return {{true,0},&style_type};
    }
    ObjectResult NewStringUtf8(const char*) override {throw std::runtime_error("unexpected string allocation");}
    ObjectResult AllocateObject(const char*,const char*) override {throw std::runtime_error("unexpected managed allocation");}
};
template<class F>void Throws(F&& f){bool threw=false;try{f();}catch(const std::runtime_error&){threw=true;}Check(threw);}
void End(){TrackedEntities().Disposed(token);TrackedEntities().Forget(token);}
void Test() {
    Objects objects;PresentationBindings b;
    b.references={M(Keep),M(Target),M(Release),{}, {}};
    b.entity.owner_world=M(World);b.entity.loaded=M(Loaded);b.entity.component=M(Component);
    b.model.animator=M(Animator);b.model.changing=M(Changing);
    b.profile_inputs.animator_initialized=M(Initialized);b.animation.controller=M(Controller);
    b.mesh.vertex_count=M(Vertices);
    b.events.has_consumer=true;b.events.consumer_offset=offsetof(EventComponent,consumer);
    PresentationContext context(objects,b);
    Check(types==0 && roots.empty() && mesh_reads==0);
    const auto& resources=context.Resources();
    Check(&resources==&context.Resources() && &resources.Mesh()==&context.Resources().Mesh());
    const auto count=resources.Mesh().Counts(&mesh).vertices;
    Check(count.read.returned && count.value==42 && mesh_reads==1);
    Check(!context.ReadAnimator({}));Throws([&]{context.NpcMotionComponent({},world);});Check(types==0);
    token=TrackedEntities().Track(entity);
    // A tracked official actor is not necessarily owned by this client.
    // Facing must reject it before reading a component or issuing a write.
    const auto before_facing=components;
    Throws([&]{context.FaceEntity(token,world,{5,0,5});});
    Check(components==before_facing && types==0 && roots.empty());
    auto view=context.ReadAnimator(token);
    Check(view && view->component==&model && view->animator==&animator && view->controller==&controller);
    Check(types==1 && roots.size()==1);
    Check(context.ReadAnimator(token).has_value() && types==1); // retained Type, no new lookup
    changing=true;const auto before=initialized_reads;
    Check(!context.ReadAnimator(token) && initialized_reads==before);changing=false;
    initialized=false;const auto reads=controller_reads;
    Check(!context.ReadAnimator(token) && controller_reads==reads);initialized=true;
    missing_controller=true;Check(!context.ReadAnimator(token));missing_controller=false;
    Throws([&]{context.NpcMotionComponent(token,reinterpret_cast<GameWorld*>(&other_world));});Check(types==1);
    loaded=false;Throws([&]{context.NpcMotionComponent(token,world);});loaded=true;Check(types==1);
    Check(context.NpcMotionComponent(token,world)==&style && types==2 && roots.size()==2);
    Check(context.NpcMotionComponent(token,world)==&style && types==2);
    Check(context.ReadEventReceiver(token,world)==&receiver_a);
    event_component.consumer=&receiver_b;Check(context.ReadEventReceiver(token,world)==&receiver_b);
    event_component.consumer=nullptr;Check(!context.ReadEventReceiver(token,world));event_component.consumer=&receiver_a;
    Check(!context.ReadEventReceiver(token,reinterpret_cast<GameWorld*>(&other_world)));
    loaded=false;Check(!context.ReadEventReceiver(token,world));loaded=true;
    component_callback=[]{End();};Check(!context.ReadEventReceiver(token,world));component_callback={};
    token=TrackedEntities().Track(entity);
    component_callback=[]{End();};const auto initialized_before=initialized_reads;
    Check(!context.ReadAnimator(token) && initialized_reads==initialized_before);component_callback={};
    const auto stale=token;token=TrackedEntities().Track(entity);const auto component_before=components;
    Check(!context.ReadAnimator(stale) && components==component_before);
    initialized_callback=[]{End();};const auto controller_before=controller_reads;
    Check(!context.ReadAnimator(token) && controller_reads==controller_before);initialized_callback={};
    token=TrackedEntities().Track(entity);
    Check(context.Close() && roots.empty());
    // A type lookup can reenter lifecycle notification. Do not read its ended entity.
    PresentationContext reentrant(objects,b);
    type_callback=[]{End();};const auto component_end=components;
    Check(!reentrant.ReadAnimator(token) && components==component_end);type_callback={};
    Check(reentrant.Close() && roots.empty());
    Check(context.Close() && roots.empty());
    // Factory survives its Context wrapper; each session owns independent input
    // roots and SDK call wrappers, with no managed work during construction.
    const auto animations=[&]{PresentationContext temporary(objects,b);return temporary.Animations();}();
    token=TrackedEntities().Track(entity);
    auto first=animations(token),second=animations(token);
    Check(roots.empty() && first->Owner().generation==token.generation);
    Check(&first->Control()!=&second->Control());
    Check(first->Entities().Read(entity).loaded.value);
    Check(first->Models().Read(reinterpret_cast<CharacterModelComponent*>(&model)).animator.value==&animator);
    Check(first->Inputs().AnimatorInitialized(&animator).value);
    Check(first->Control().Controller(&animator).object==&controller);
    first->Resources().RetainOwner();second->Resources().RetainOwner();Check(roots.size()==2);
    Check(first->Close() && roots.size()==1);
    Check(second->Resources().Component(AnimationComponent::Model,objects).object==&model);
    End();Check(second->Close() && roots.empty());
    // A model provider is independent of its Context wrapper. Its factory
    // owners do not share root collections or dispose one another's objects.
    b.references.load_asset=M(Load);b.references.release_asset=M(Unload);
    b.lifetime={M(NativeAlive),M(Destroy)};
    const auto models=[&]{PresentationContext temporary(objects,b);return temporary.Models();}();
    const auto type_count=types;
    auto roots_a=models->CreateRoots(),roots_b=models->CreateRoots();
    auto leases=models->CreateAssetLeases();auto native=models->CreateNativeObjects();
    auto look=models->CreateLookSupport();
    Check(roots.empty() && types==type_count && destroys==0 && loads==0);
    Check(!models->CanOverrideBones() && !models->CanOverrideMeshAndMaterials());
    Check(models->Entities().Read(entity).loaded.value);
    Check(models->Models().Read(reinterpret_cast<CharacterModelComponent*>(&model)).animator.value==&animator);
    Check(models->ComponentType(objects).object==&model_type && types==type_count+1);
    const auto a=roots_a->Keep(&mesh),c=roots_b->Keep(&mesh);
    Check(roots.size()==2 && roots_a->Release(a).complete && roots.size()==1);
    Check(roots_b->Target(c).object==&mesh && roots_b->Release(c).complete && roots.empty());
    const auto lease=leases->Acquire(&style,&model_type,false);
    Check(loads==1 && roots.size()==1 && leases->Object(lease)==&mesh);
    const auto object=native->Adopt(&mesh);
    Check(roots.size()==2 && models->Alive(&mesh).value);
    Check(!native->Close(object).complete && destroys==1 && !models->Alive(&mesh).value);
    Check(native->Close(object).complete && roots.size()==1 && releases==0);
    Check(leases->Release(lease).complete && roots.empty() && releases==1);
    Check(look->Close().complete && roots.empty());
    b.world_map.group_manager=M(Group);b.world_map.map=M(Map);b.world_map.identity.fill(M(MapKey));
    b.placement={M(Player),M(World),M(GameObject),M(Transform),M(Position),M(Rotation)};
    b.errors={"fixture diagnostic"};PresentationContext scene(objects,b);
    Check(scene.BindingErrors()==b.errors && world_reads==0);
    auto scene_roots=scene.CreateRoots();const auto scene_root=scene_roots->Keep(world);
    Check(roots.size()==1 && scene.Close() && roots.size()==1); // caller-owned collection
    Check(scene_roots->Release(scene_root).complete && roots.empty());
    WorldFrame frame;frame.world=world;frame.adventure_phase={&phase,{true,0}};frame.disposing={false,{true,0}};
    const auto map_view=scene.ReadMap(frame);const auto placement=scene.ReadPlayerPlacement(frame);
    Check(map_view.map.value==&map && map_view.identity[0].value==23 && placement.ready);
    Check(placement.position.y==2 && placement.rotation.w==1 && world_reads==2);
    const auto placed=TrackedEntities().Track(entity);
    Check(scene.ReadEntityPlacement(placed,world).ready);
    Check(!scene.ReadEntityPlacement(placed,reinterpret_cast<GameWorld*>(&other_world)).ready);
    TrackedEntities().Disposed(placed);Check(!scene.ReadEntityPlacement(placed,world).ready);TrackedEntities().Forget(placed);
    frame.disposing.value=true;
    Check(!scene.ReadMap(frame).map.read.returned && !scene.ReadPlayerPlacement(frame).ready && world_reads==2);
}
}
int main(){try{Test();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
