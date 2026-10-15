#include "../../src/runtime/RuntimeClients.hpp"
#include <functional>
#include <iostream>
#include <thread>

namespace {
using namespace hksr::api;
void Check(bool value){if(!value)throw std::runtime_error("runtime subscription isolation failed");}
struct Client final:RuntimeClient {
    unsigned frames=0,stops=0;
    bool fail=false,done=false;
    std::function<void()> during;
    std::function<void()> before,after;
    void Prepare(RuntimeMetadata&,RuntimeCapabilities) override {Check(false);}
    void BeforeFrame(const WorldFrame&,bool) override {if(before)before();}
    void AfterFrame(const WorldFrame&,bool) override {if(after)after();}
    bool Frame(const WorldFrame&,bool stopping) override {
        ++frames;if(stopping)++stops;
        if(during)during();
        if(fail)throw std::runtime_error("client frame");
        return done;
    }
};
}
int main()try {
    detail::RuntimeClients clients;
    auto a=std::make_shared<Client>(),b=std::make_shared<Client>();
    auto ra=clients.Add(a),rb=clients.Add(b);
    unsigned prepared=0,errors=0;
    auto prepare=[&](RuntimeClient&){++prepared;};
    auto report=[&](const char*){++errors;};
    // Registration is not preparation, and no pending client receives frames.
    clients.Deliver({},report);Check(!a->frames && !b->frames);
    clients.PreparePending(prepare);Check(prepared==2 && ra->Read().prepared && rb->Read().prepared);
    clients.PreparePending(prepare);Check(prepared==2);
    ra->stop=true;Check(ra->Read().stop_requested && ra->Read().phase==RuntimeHostPhase::Stopping);
    clients.Deliver({},report);
    Check(a->stops==1 && !b->stops && ra->Read().phase==RuntimeHostPhase::Stopping);
    // A draining client remains owned through multiple frames. A throw from
    // the first client neither releases its ownership nor skips the second.
    a->fail=true;clients.Deliver({},report);
    Check(errors==1 && ra->Read().frame_failures==1 && b->frames==2);
    a->fail=false;a->done=true;std::weak_ptr<Client> life=a;a.reset();
    clients.Deliver({},report);Check(life.expired() && ra->Read().phase==RuntimeHostPhase::Stopped && b->frames==3);
    // Pending cancellation has no preparation or engine cleanup obligation.
    auto cancelled=std::make_shared<Client>();std::weak_ptr<Client> cancelled_life=cancelled;
    auto rc=clients.Add(cancelled);cancelled.reset();rc->stop=true;
    clients.PreparePending(prepare);
    Check(prepared==2 && cancelled_life.expired() && rc->Read().phase==RuntimeHostPhase::Stopped);
    // All independent preparations are attempted even if one rejects inputs.
    auto bad=std::make_shared<Client>(),good=std::make_shared<Client>();
    auto bad_record=clients.Add(bad),good_record=clients.Add(good);
    clients.PreparePending([&](RuntimeClient& current){if(&current==bad.get())throw std::runtime_error("bad prepare");});
    Check(bad_record->Read().phase==RuntimeHostPhase::Failed && bad_record->Read().preparation_error=="bad prepare");
    Check(good_record->Read().phase==RuntimeHostPhase::Running);
    // Registration and stop can be requested while another callback executes;
    // no registry/status mutex is held across client code. The new client is
    // deferred until a later preparation boundary, regardless of insertion order.
    auto late=std::make_shared<Client>();std::shared_ptr<detail::RuntimeClientRecord> late_record;
    b->during=[&]{
        std::thread other([&]{late_record=clients.Add(late);good_record->stop=true;});other.join();
    };
    clients.Deliver({},report);b->during={};
    Check(late_record && late_record->Read().phase==RuntimeHostPhase::Starting && !late->frames);
    Check(good->stops==0); // Stop requested during a batch applies next frame.
    // Failed late attachment only rejects its pending registrations.
    clients.FailPending("attach failed");
    Check(late_record->Read().phase==RuntimeHostPhase::Failed && rb->Read().phase==RuntimeHostPhase::Running);
    auto next=clients.Add(late);clients.PreparePending(prepare);clients.Deliver({},report);
    Check(next->Read().phase==RuntimeHostPhase::Running && late->frames==1);
    // Stop requested while Prepare is executing is not discarded. A prepared
    // client still receives Frame(stop=true) to drain its registered callbacks.
    auto raced=std::make_shared<Client>();auto race_record=clients.Add(raced);
    clients.PreparePending([&](RuntimeClient& current){
        Check(&current==raced.get());
        std::thread stop([&]{race_record->stop=true;});stop.join();
    });
    Check(race_record->Read().prepared && race_record->Read().phase==RuntimeHostPhase::Stopping);
    clients.Deliver({},report);Check(raced->stops==1);
    // A frame-hook startup failure retires prepared and pending records alike.
    auto pending=clients.Add(std::make_shared<Client>());
    clients.FailStartup("hook failed");
    Check(rb->Read().phase==RuntimeHostPhase::Failed && pending->Read().preparation_error=="hook failed");
    Check(!clients.HasPending());
    detail::RuntimeClients observed;
    std::vector<int> order;
    auto producer=std::make_shared<Client>(),observer=std::make_shared<Client>();
    producer->before=[&]{order.push_back(1);};producer->during=[&]{order.push_back(3);};producer->after=[&]{order.push_back(6);};
    observer->before=[&]{order.push_back(2);};observer->during=[&]{order.push_back(4);};observer->after=[&]{order.push_back(5);};
    auto producer_record=observed.Add(producer),observer_record=observed.Add(observer);
    observed.PreparePending(prepare);producer->fail=true;
    observed.Deliver({},report);
    Check(order==std::vector<int>{1,2,3,4,5,6});
    Check(producer_record->Read().frame_failures==1 && observer_record->Read().frame_failures==0);
    observer->done=true;observer->after=[]{throw std::runtime_error("observer cleanup");};
    observed.Deliver({},report);Check(observer_record->Read().phase!=RuntimeHostPhase::Stopped);
    observer->after={};observed.Deliver({},report);Check(observer_record->Read().phase==RuntimeHostPhase::Stopped);
    std::cout<<"Per-client prepare/stop/failure isolation, late registration and callback reentry passed; no game.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
