#include <hksr/animation/ActionRelations.hpp>
#include <limits>

namespace hksr::sdk_v1 {
ReadyTransitionMatch FindReadyTransition(
    const CatalogBatch<ReadyTransitionInput>& character,
    const std::vector<ReadyTransitionInput>& defaults,int from,int to) {
    ReadyTransitionMatch result;
    if(from<0 || to<0 || from==std::numeric_limits<int>::max() || to==std::numeric_limits<int>::max()) {
        result.resolution=ReadyTransitionResolution::InvalidIndex;
        return result;
    }
    const auto scan=[&](const auto& rows,ReadyTransitionTable table) {
        for(std::size_t i=0;i<rows.size();++i) {
            const auto& row=rows[i];
            if(!row.from.available || !row.to.available) {
                result={ReadyTransitionResolution::UnreadKey,table,i,{}};
                return true;
            }
            if(row.from.value==from+1 && row.to.value==to+1) {
                result={ReadyTransitionResolution::Found,table,i,row};
                return true;
            }
        }
        return false;
    };
    if(scan(character.rows,ReadyTransitionTable::Character))return result;
    if(!character.enumerated || !character.issues.empty()) {
        result.resolution=ReadyTransitionResolution::IncompleteTable;
        result.table=ReadyTransitionTable::Character;
        return result;
    }
    scan(defaults,ReadyTransitionTable::OfficialDefault);
    return result;
}

std::vector<PreparationSequence> FindPreparationSequences(const AbilitySources& sources) {
    const auto ordinal=[](const auto& path) {
        return path.size()==1 && path[0].field=="OnStart" && path[0].selection==ConfigSelection::Index
            ?path[0].index:-1;
    };
    std::vector<PreparationSequence> result;
    for(const auto& continuation:sources.skill_continuations) {
        const auto end=ordinal(continuation.path);
        if(end<0 || !continuation.control.available)continue;
        const SkillMappingSource* mapping=nullptr;
        int begin=-1;
        for(const auto& row:sources.skill_mappings) {
            const auto index=ordinal(row.path);
            if(row.ability==continuation.ability && index>=0 && index<end && index>begin &&
                row.control.available && row.control.value==continuation.control.value) {
                mapping=&row;begin=index;
            }
        }
        if(!mapping)continue;
        PreparationSequence sequence{*mapping,continuation,{}};
        for(const auto& row:sources.animations) {
            const auto index=ordinal(row.path);
            if(row.ability==continuation.ability && index>begin && index<end)sequence.animations.push_back(row);
        }
        result.push_back(std::move(sequence));
    }
    return result;
}
}
