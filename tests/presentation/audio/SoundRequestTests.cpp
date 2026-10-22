#include <hksr/presentation/audio/OwnedSoundRequest.hpp>
#include <array>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>

namespace {
using namespace hksr::api;
void Check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
const SoundDelegateLayout layout{32,48,96}; // relocated, not the game offsets
struct Delegate {alignas(void*) std::array<unsigned char,128> memory{};};
struct Entry {void* manager;void* emitter;void* began;void* ended;std::uint32_t id;void* banks=nullptr;bool pending=true,playing=false;};
struct Manager {void* unused=nullptr;void* queue=nullptr;};
struct Fixture {
    std::map<int,void*> roots;
    int next=0,posts=0,stops=0,alive_reads=0,fail_alive_at=0,fail_retain_at=0,retains=0;
    Manager manager;int queue=1,name=2,emitter=3,callback_info=4,banks=5;
    bool alive=true,sync_begin=false,sync_end=false,empty_end=false,reject=false,stop_callback=false;
    std::uint32_t next_event=10;
    std::deque<Delegate> delegates;
    std::deque<Entry> entries;
    std::vector<std::uint32_t> playing_ids;
    std::vector<std::uint32_t> stopped;
    Fixture(){manager.queue=&queue;}
}* fixture;
template<class T>Method M(T entry){return {reinterpret_cast<void*>(entry),nullptr};}
int __fastcall Keep(void* value,int,int,void*) {
    if(++fixture->retains==fixture->fail_retain_at)return 0;
    const auto id=++fixture->next;fixture->roots[id]=value;return id;
}
void* __fastcall Target(int id,void*){return fixture->roots.at(id);}
void __fastcall Free(int id,void*){fixture->roots.erase(id);}
bool __fastcall Alive(void* object,void*) {
    Check(object==&fixture->emitter,"unexpected emitter");
    return fixture->alive && ++fixture->alive_reads!=fixture->fail_alive_at;
}
template<class... Args>void Invoke(void* object,Args... args) {
    void* entry=nullptr;void* cookie=nullptr;
    std::memcpy(&entry,static_cast<char*>(object)+layout.invoke_impl,sizeof(entry));
    std::memcpy(&cookie,static_cast<char*>(object)+layout.method_code,sizeof(cookie));
    reinterpret_cast<void(__fastcall*)(void*,Args...,void*)>(entry)(cookie,args...,nullptr);
}
void Begin(const Entry& value){Invoke(value.began,value.id);}
void End(const Entry& value,bool info=true){Invoke(value.ended,value.id,std::uint32_t{1},info?static_cast<void*>(&fixture->callback_info):nullptr);}
std::uint32_t __fastcall Post(void* manager,void* name,void* emitter,void* ended,void* began,std::uint32_t flags,void*) {
    Check(manager==&fixture->manager && name==&fixture->name && emitter==&fixture->emitter && flags==1,"post ABI/flags");
    ++fixture->posts;
    if(fixture->reject)return 0;
    const Entry value{manager,emitter,began,ended,fixture->next_event++,&fixture->banks,!fixture->sync_begin,fixture->sync_begin};fixture->entries.push_back(value);
    if(fixture->sync_begin)Begin(value);
    if(fixture->sync_end)End(value,!fixture->empty_end);
    return value.id;
}
void __fastcall Stop(void* manager,std::uint32_t id,float seconds,void*) {
    Check(manager==&fixture->manager && seconds==0,"stop ABI");
    ++fixture->stops;fixture->stopped.push_back(id);
    if(fixture->stop_callback)for(const auto& entry:fixture->entries)if(entry.id==id)End(entry);
}
void* __fastcall First(void*,void*){for(auto& e:fixture->entries)if(e.pending)return &e;return nullptr;}
void* __fastcall Next(void* node,void*) {
    bool after=false;
    for(auto& e:fixture->entries){if(after && e.pending)return &e;if(&e==node)after=true;}return nullptr;
}
void* __fastcall Item(void* node,void*){return node;}
void __fastcall Remove(void*,void* node,void*) {
    auto& e=*static_cast<Entry*>(node);Check(e.pending,"remove only queued node");e.pending=false;
    ++fixture->stops;fixture->stopped.push_back(e.id);
}
void __fastcall ReleaseBanks(void* manager,void* banks,void*) {
    Check(manager==&fixture->manager && banks==&fixture->banks,"bank release ownership");
}
void* __fastcall Cache(void* manager,void*){return manager;}
void* __fastcall PlayingIds(void*,std::uint32_t id,void*) {
    auto& ids=fixture->playing_ids;ids.clear();std::uint32_t index=100;
    for(const auto& e:fixture->entries){if(e.playing && e.id==id)ids.push_back(index);++index;}return &ids;
}
void* __fastcall Info(void*,std::uint32_t id,void*) {
    if(id<100 || id-100>=fixture->entries.size())return nullptr;
    auto& e=fixture->entries.at(id-100);return e.playing?&e:nullptr;
}
int __fastcall Count(void* value,void*){return static_cast<int>(static_cast<std::vector<std::uint32_t>*>(value)->size());}
std::uint32_t __fastcall IdAt(void* value,int index,void*){return static_cast<std::vector<std::uint32_t>*>(value)->at(index);}
void __fastcall StopPlaying(std::uint32_t action,std::uint32_t playing,int milliseconds,void*) {
    Check(action==0 && milliseconds==0 && playing>=100,"scoped playing stop ABI");
    auto& e=fixture->entries.at(playing-100);Check(e.playing,"only current playing ID");
    e.playing=false;++fixture->stops;fixture->stopped.push_back(e.id);
    if(fixture->stop_callback)End(e);
}
SoundRequestServices Services(bool stop_available=true) {
    const ReferenceMethods references{M(Keep),M(Target),M(Free),{}, {}};
    auto allocate=[] {
        fixture->delegates.emplace_back();return ObjectResult{{true,0},fixture->delegates.back().memory.data()};
    };
    const SoundTrackingBinding tracking{
        {M(First),M(Next),M(Item),stop_available?M(Remove):Method{},M(ReleaseBanks),M(Cache),M(PlayingIds),M(Info),M(Count),M(IdAt),stop_available?M(StopPlaying):Method{}},
        {offsetof(Manager,queue),offsetof(Entry,ended),offsetof(Entry,emitter),offsetof(Entry,banks),offsetof(Entry,ended),offsetof(Entry,emitter)}};
    return {references,SoundAccess({M(Post),M(Stop),M(Alive)}),{{allocate,layout},{allocate,layout}},{},tracking};
}
void Start(OwnedSoundRequest& request){request.Start(&fixture->manager,&fixture->name,&fixture->emitter);}
void Run() {
    {
        Fixture f;fixture=&f;OwnedSoundRequest request(Services());Start(request);
        Check(request.Status().accepted && !request.Status().started && !request.Status().ended,"ID must not imply playback");
        request.Cancel();Check(f.stops==0,"Cancel must be data-only");request.Service();
        Check(request.Status().closed && request.Status().cancelled && !request.Status().ended && f.stops==1 && f.roots.empty(),"pending cancellation");
        Begin(f.entries[0]);End(f.entries[0]);request.Service();
        Check(!request.Status().started && !request.Status().ended && f.stops==1,"late callback after pending cancellation");
    }
    {
        Fixture f;fixture=&f;f.sync_begin=true;f.sync_end=true;
        OwnedSoundRequest request(Services());Start(request);
        Check(request.Status().started && request.Status().ended && request.Status().closed && f.roots.empty(),"synchronous begin/end");
        request.Cancel();request.Service();Check(f.stops==0,"natural completion must not stop a reused ID");
    }
    {
        Fixture f;fixture=&f;f.sync_end=true;f.empty_end=true;
        OwnedSoundRequest request(Services());Start(request);
        Check(!request.Status().started && request.Status().ended && request.Status().closed && !request.Status().error.empty(),"failed playback callback");
    }
    {
        Fixture f;fixture=&f;OwnedSoundRequest first(Services()),second(Services());Start(first);Start(second);
        std::thread callbacks([&]{Begin(f.entries[1]);End(f.entries[1]);});callbacks.join();
        second.Service();first.Cancel();first.Service();
        Check(first.Status().closed && !first.Status().started && second.Status().started && second.Status().ended,"same event overlap");
        Check(f.stopped==std::vector<std::uint32_t>{f.entries[0].id} && f.roots.empty(),"request-specific ID stop");
    }
    {
        Fixture f;fixture=&f;OwnedSoundRequest request(Services());Start(request);
        Invoke(f.entries[0].began,std::uint32_t{999});
        Invoke(f.entries[0].ended,f.entries[0].id,std::uint32_t{0x2000},static_cast<void*>(&f.callback_info));
        request.Service();Check(request.Status().foreign_callbacks==1 && !request.Status().started && !request.Status().ended,"unrelated callback");
        f.alive=false;request.Service();Check(request.Status().cancelled && request.Status().closed && f.stops==1,"dead native emitter");
    }
    {
        Fixture f;fixture=&f;f.sync_begin=true;f.stop_callback=true;
        OwnedSoundRequest request(Services());Start(request);request.Service(false);
        Check(request.Status().cancelled && request.Status().ended && request.Status().closed && f.stops==1,"model generation loss");
    }
    {
        Fixture f;fixture=&f;auto services=Services();services.access=SoundAccess({{},M(Stop),M(Alive)});
        OwnedSoundRequest request(std::move(services));Start(request);request.Cancel();request.Service();
        Check(!request.Status().closed && !request.Status().submission.read.returned && f.stops==0,"unknown submission retained");
        // A callback supplies an identity after an ambiguous submission. The
        // service can cancel that identity, but must not invent acceptance.
        Invoke(f.delegates.at(0).memory.data(),std::uint32_t{77});request.Service();
        Check(request.Status().closed && !request.Status().accepted && f.stopped.empty(),"late identity does not authorize ID-wide stop");
    }
    {
        Fixture f;fixture=&f;f.stop_callback=true;f.sync_begin=true;
        OwnedSoundRequest first(Services()),second(Services());Start(first);f.next_event=first.Status().submission.value;Start(second);
        first.Cancel();first.Service();second.Service();
        Check(first.Status().closed && !second.Status().ended && !second.Status().closed && f.stops==1,"same logical ID has independent playback cancellation");
        second.Cancel();second.Service();Check(second.Status().closed && f.stops==2,"second playback retained its own stop");
    }
    {
        Fixture f;fixture=&f;OwnedSoundRequest first(Services()),second(Services());Start(first);f.next_event=first.Status().submission.value;Start(second);
        first.Cancel();first.Service();second.Service();
        Check(first.Status().closed && !second.Status().closed && f.entries[1].pending,"same-ID pending cancellation leaves other node queued");
        second.Cancel();second.Service();Check(f.stops==2 && f.roots.empty(),"pending nodes drained separately");
    }
    {
        Fixture f;fixture=&f;OwnedSoundRequest request(Services());Start(request);
        f.entries[0].pending=false;request.Service();
        Check(request.Status().dropped && request.Status().closed && !request.Status().ended && !request.Status().error.empty(),"silent bank drop is a failure, not a successful end");
        Check(f.stops==0 && f.roots.empty(),"dropped request needs no stop");
    }
    {
        Fixture f;fixture=&f;f.sync_begin=true;OwnedSoundRequest playing(Services());Start(playing);
        f.sync_begin=false;f.next_event=playing.Status().submission.value;OwnedSoundRequest pending(Services());Start(pending);
        f.entries[1].pending=false;pending.Service();
        Check(pending.Status().dropped && !playing.Status().closed,"foreign same-ID playback cannot conceal pending drop");
        playing.Cancel();playing.Service();Check(f.stops==1 && f.roots.empty(),"drop did not stop foreign playback");
    }
    {
        Fixture f;fixture=&f;auto services=Services();services.tracking.methods.first={};
        OwnedSoundRequest request(std::move(services));Start(request);
        Check(!request.Status().dropped && !request.Status().closed && !request.Status().error.empty(),"failed lookup is not an empty queue");
        End(f.entries[0]);request.Service();Check(request.Status().closed && f.roots.empty(),"real callback closes after lookup failure");
    }
    {
        Fixture f;fixture=&f;OwnedSoundRequest first(Services()),second(Services());Start(first);f.next_event=first.Status().submission.value;Start(second);
        auto& e=f.entries[0];e.pending=false;e.playing=true;Begin(e);first.Service();
        first.Cancel();first.Service();second.Service();
        Check(first.Status().closed && f.entries[1].pending && !second.Status().closed,"queue-to-playing transition keeps exact ownership");
        second.Cancel();second.Service();Check(f.stops==2 && f.roots.empty(),"mixed pending/playing cancellation");
    }
    {
        Fixture f;fixture=&f;f.alive=false;OwnedSoundRequest request(Services());Start(request);
        Check(f.posts==0 && request.Status().closed && f.roots.empty(),"no listener fallback");
    }
    {
        Fixture f;fixture=&f;bool current=true;auto services=Services();services.current=[&]{return current;};
        OwnedSoundRequest request(std::move(services));Start(request);current=false;request.Service();
        Check(request.Status().cancelled && request.Status().closed && f.stops==1,"original source generation binding");
        OwnedSoundRequest stale(Services()); // explicit caller invalidation also works before submission
        stale.Service(false);Start(stale);Check(f.posts==1,"invalid owner cannot submit");
    }
    {
        Fixture f;fixture=&f;f.fail_alive_at=2;OwnedSoundRequest request(Services());Start(request);
        Check(f.posts==0 && !request.Status().submission.attempted && request.Status().closed && f.roots.empty(),"lost emitter before submission");
    }
    {
        Fixture f;fixture=&f;f.fail_retain_at=5;OwnedSoundRequest request(Services());Start(request);
        Check(f.posts==0 && request.Status().closed && f.roots.empty(),"partial callback allocation cleanup");
    }
    {
        Fixture f;fixture=&f;f.reject=true;OwnedSoundRequest request(Services());Start(request);
        Check(!request.Status().accepted && request.Status().closed && f.roots.empty(),"rejected post");
    }
    {
        Fixture f;fixture=&f;OwnedSoundRequest request(Services(false));Start(request);request.Cancel();request.Service();request.Service();
        Check(!request.Status().closed && !request.Status().stop.returned && !request.Status().error.empty(),"unknown stop cannot claim closure");
        End(f.entries[0]);request.Service();Check(request.Status().closed && f.roots.empty(),"late end resolves uncertain stop");
    }
    {
        Fixture f;fixture=&f;OwnedSoundRequest request(Services());request.Cancel();request.Service();Start(request);
        Check(request.Status().closed && f.posts==0 && f.roots.empty(),"cancel before start");
    }
    {
        Fixture f;fixture=&f;auto services=Services();int count=0;SoundCallbacks* self=nullptr;
        SoundCallbacks callbacks(services.callbacks,services.references,[&](SoundCallbackEvent) {++count;Check(self->Close(),"reentrant close");throw std::runtime_error("sink");});self=&callbacks;
        const auto objects=callbacks.Create();Invoke(objects.began.object,std::uint32_t{7});Invoke(objects.ended.object,std::uint32_t{7},std::uint32_t{1},static_cast<void*>(nullptr));
        Check(count==1 && f.roots.empty(),"inert delegate and exception isolation");
    }
    std::vector<DeclaredField> fields{
        {"System","Delegate","method_ptr","System.IntPtr",32,1,true},
        {"System","Delegate","invoke_impl","System.IntPtr",48,1,true},
        {"System","Delegate","method_code","System.IntPtr",96,1,true}};
    Check(ResolveSoundDelegateLayout460(fields).method_code==96,"declaration offsets");
    fields[1].offset=32;bool rejected=false;
    try{ResolveSoundDelegateLayout460(fields);}catch(const std::exception&){rejected=true;}
    Check(rejected,"overlapping native delegate fields");
}
}
int main() {
    try{Run();std::cout<<"Sound requests: real access/bridge/request path with fake engine; no game calls.\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
