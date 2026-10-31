#include "catalog/CatalogReader.hpp"

namespace hksr::sdk_v1 {
OfficialCatalog::OfficialCatalog(MethodResolver& methods,ObjectMetadata& metadata,ManagedObjects& objects,
    ReferenceMethods references,ManagedArrays arrays,ComponentConfig text)
    :owner_(new std::shared_ptr<State>(std::make_shared<State>(methods,metadata,objects,references,arrays,text))),state_(owner_->get()){}
OfficialCatalog::~OfficialCatalog() {delete owner_;}
std::size_t OfficialCatalog::UnresolvedReferences() const { return state_->pending.size(); }
ActionDataVersion OfficialCatalog::ReadActionDataVersion() {
    auto& s=*state_;State::Scope scope(s);ActionDataVersion out;
    out.build=s.Text(scope.Keep(s.Call(s.Static("VersionData","get_BuildId","System.String"))));
    auto* version=scope.Keep(s.StaticValue(scope,"RPG.Client","GlobalVars","s_VersionData"));
    auto* update=Read<void*>(version,Field(s.metadata.ReadObjectFields(version),"UpdateVersionInfoIns",
        detail::catalog_bindings::update_version_type));
    Require(update,"runtime resource version not initialized");
    const auto query=s.methods.Resolve("",detail::catalog_bindings::update_version_type,
        detail::catalog_bindings::selected_design_version,"System.UInt32",{});
    Require(native::Call(query,&out.design,update).returned,"selected design revision unavailable");
    Require(!out.build.empty() && out.design!=0,"action data identity incomplete");
    return out;
}
}
