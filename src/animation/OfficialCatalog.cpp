#include "catalog/CatalogReader.hpp"

namespace hksr::sdk_v1 {
OfficialCatalog::OfficialCatalog(MethodResolver& methods,ObjectMetadata& metadata,ManagedObjects& objects,
    ReferenceMethods references,ManagedArrays arrays,ComponentConfig text)
    :owner_(new std::shared_ptr<State>(std::make_shared<State>(methods,metadata,objects,references,arrays,text))),state_(owner_->get()){}
OfficialCatalog::~OfficialCatalog() {delete owner_;}
std::size_t OfficialCatalog::UnresolvedReferences() const { return state_->pending.size(); }
}
