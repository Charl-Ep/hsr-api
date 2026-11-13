#include <hksr/entity/MapNpcInputs.hpp>
#include "NpcInputConstruction.hpp"
#include "../../adapters/entity/NpcInputBindings.hpp"
#include <stdexcept>

namespace hksr::api {
struct MapNpcInputs::State {
    detail::NpcInputConstruction input;
    explicit State(ReferenceMethods methods) : input(methods) {}
};
MapNpcInputs::MapNpcInputs(State* state) : state_(state) {}
MapNpcInputs::~MapNpcInputs() {
    delete state_;
}
std::unique_ptr<MapNpcInputs> MapNpcInputs::Prepare(RuntimeMetadata& metadata, ReferenceMethods references,
                                                    std::uint32_t id, const char* name, Position3 position,
                                                    Rotation4 rotation) {
    const auto ops = detail::ResolveNpcInputOperations(metadata);
    auto state = std::make_unique<State>(references);
    auto result = std::unique_ptr<MapNpcInputs>(new MapNpcInputs(state.get()));
    state.release();
    result->state_->input.Prepare(ops, id, name, position, rotation);
    return result; // retains partial failure receipts as well as successful inputs
}
NpcInputStatus MapNpcInputs::Status() const {
    return state_->input.Status();
}
MapNpcDefinition* MapNpcInputs::Definition() {
    return static_cast<MapNpcDefinition*>(state_->input.Definition());
}
NpcDataRow* MapNpcInputs::Row() {
    return static_cast<NpcDataRow*>(state_->input.Row());
}
bool MapNpcInputs::Release() {
    return state_->input.Release();
}
}
