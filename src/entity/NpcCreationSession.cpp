#include <hksr/entity/EntityCreationSession.hpp>

namespace hksr::sdk_v1 {
// Published map-NPC API delegates lifetime ownership to the shared core.
struct NpcCreationSession::State {
    EntityCreationSession session;
    State(const EntityOwner& owner,Method factory,EntityAccessMethods access,ReferenceMethods refs,EntityCreationScope::BirthObserver observer)
        :session(owner,factory,{},nullptr,std::move(access),refs,std::move(observer)){}
};
NpcCreationSession::NpcCreationSession(const EntityOwner& owner,Method factory,EntityAccessMethods access,ReferenceMethods refs,EntityCreationScope::BirthObserver observer)
    :state_(new State(owner,factory,std::move(access),refs,std::move(observer))){}
NpcCreationSession::~NpcCreationSession(){delete state_;}
void NpcCreationSession::Create(const NpcCreateInput& input){state_->session.Create(input);}
void NpcCreationSession::Remove(){state_->session.Remove();}
void NpcCreationSession::Service(const WorldFrame& frame){state_->session.Service(frame);}
const NpcCreationStatus& NpcCreationSession::Status() const{return state_->session.Status();}
}
