#include "../../src/presentation/PresentationClients.hpp"
#include "../../src/presentation/timeline/TimelineOwnership.hpp"
#include <iostream>
#include <thread>
#include <atomic>

using namespace hksr::api;
using namespace hksr::sdk_v1::detail;
namespace {
void Check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
struct Service {PresentationClients clients;TimelineOwnership records;};
EntityOrigin Origin(std::uintptr_t n,std::uint64_t generation=1) {
    return {{reinterpret_cast<GameEntity*>(n),generation},reinterpret_cast<GameWorld*>(100)};
}
}
int main() try {
    PresentationServiceSlot<Service> slot;std::atomic<int> built{0};
    auto create=[&]{++built;return std::make_shared<Service>();};
    std::shared_ptr<Service> a,b;
    std::thread ta([&]{a=slot.Acquire(create);}),tb([&]{b=slot.Acquire(create);});ta.join();tb.join();
    Check(a==b && built==1,"concurrent clients installed separate global services");
    int log_a=0,log_b=0,log_c=0;
    const auto ca=a->clients.Add({true,false},[&](const auto&){++log_a;});
    const auto cb=b->clients.Add({false,true},[&](const auto&){++log_b;});
    a->clients.Remember(ca,Origin(1));b->clients.Remember(cb,Origin(2));
    Check(a->clients.Owns(ca,Origin(1)) && !a->clients.Owns(cb,Origin(1)) && !a->clients.Owns(ca,Origin(1,2)),"source attribution crossed clients or generations");
    Check(!a->clients.Owns(ca,{}) && !a->clients.Owns(ca,Origin(99)),"public source attributed to client");
    Check(a->clients.Policy(Origin(1)).allow_main_camera && !b->clients.Policy(Origin(2)).allow_main_camera,"camera policies leaked between clients");
    Check(!a->clients.Policy(Origin(1)).allow_timeline_environment && b->clients.Policy(Origin(2)).allow_timeline_environment,"environment policies leaked between clients");
    bool rejected=false;try{a->clients.Remember(cb,Origin(1));}catch(const std::logic_error&){rejected=true;}
    Check(rejected,"second client stole first client's origin");
    a->clients.Log("first");Check(log_a==1 && log_b==1,"active diagnostics missing");
    a->clients.Detach(ca);a.reset();
    auto c=slot.Acquire(create);const auto cc=c->clients.Add({},[&](const auto&){++log_c;});
    c->clients.Remember(cc,Origin(3));c->clients.Remember(cc,Origin(1,2));
    Check(!c->clients.Owns(ca,Origin(1)) && !c->clients.Owns(cc,Origin(1)) && c->clients.Owns(cc,Origin(1,2)),"retired client or old incarnation attributed to new client");
    c->clients.Log("tail");
    Check(b==c && built==1 && log_a==1 && log_b==2 && log_c==1,"retirement replaced service or retained retired callback");
    Check(c->clients.Policy(Origin(1)).allow_main_camera && !c->clients.Policy(Origin(1,2)).allow_main_camera,"retired policy or reincarnation lost");
    rejected=false;try{c->clients.RequireOwner(cc,Origin(2));}catch(const std::logic_error&){rejected=true;}
    Check(rejected,"cross-client effect request accepted");
    c->clients.RequireOwner(cb,Origin(2));
    c->clients.Collect([](EntityOrigin o){return o.token.generation==1;});
    Check(c->clients.Policy(Origin(1)).allow_main_camera,"live retired tail policy was collected");
    rejected=false;try{c->clients.Policy(Origin(1,2));}catch(const std::logic_error&){rejected=true;}
    Check(rejected,"unreferenced owner policy retained indefinitely");
    b->clients.Detach(cb);c->clients.Detach(cc);b.reset();c.reset();
    Check(slot.Existing()!=nullptr && built==1,"last client destroyed process tail service");
    PresentationServiceSlot<Service> retry;
    try{retry.Acquire([]()->std::shared_ptr<Service>{throw std::runtime_error("synthetic installation failure");});}catch(const std::runtime_error&){}
    Check(!retry.Existing() && retry.Acquire(create)!=nullptr,"failed construction was published");
    std::cout<<"Shared presentation service: concurrent acquisition, client policy/trace ownership, retirement, rejoin and failed construction passed.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
