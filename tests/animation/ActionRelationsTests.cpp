#include <hksr/animation/ActionRelations.hpp>
#include <limits>
#include <stdexcept>

namespace {
using namespace hksr::api;
void Check(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
ReadyTransitionInput Edge(int from,int to,const char* state,int source=0) {
    ReadyTransitionInput row;row.source_index=source;
    row.from={true,from,{}};row.to={true,to,{}};row.state={true,state,{}};
    row.transition_in={true,0.25f,{}};row.reverse_outro={true,0.5f,{}};
    return row;
}
std::vector<ConfigStep> Path(int index) {
    return {{"FixtureAbility","OnStart",ConfigSelection::Index,index,{}}};
}
AnimationTaskSource Animation(int index,const char* state) {
    AnimationTaskSource row;row.ability="setup";row.path=Path(index);
    row.state={true,state,{}};row.target_alias={true,"Caster",{}};row.enabled={true,true,{}};
    return row;
}
}
void TestActionRelations() {
    CatalogBatch<ReadyTransitionInput> character;
    character.enumerated=true;
    character.rows={Edge(4,9,"First",11),Edge(4,9,"Second",12)};
    std::vector<ReadyTransitionInput> defaults{Edge(4,9,"Fallback",31)};
    auto match=FindReadyTransition(character,defaults,3,8);
    Check(match.resolution==ReadyTransitionResolution::Found && match.table==ReadyTransitionTable::Character &&
        match.position==0 && match.row->source_index==11 && match.row->state.value=="First","ordered first match");
    character.rows.front().state.value.clear();
    Check(match.row->state.value=="First","result owns declaration snapshot");
    match=FindReadyTransition(character,defaults,3,8);
    Check(match.row && match.row->state.available && match.row->state.value.empty() &&
        match.table==ReadyTransitionTable::Character,"empty match suppresses default");
    character.rows.front().state={false,{},"unread state"};
    character.rows.front().transition_in={true,-2,{}};
    character.rows.front().reverse_outro={true,std::numeric_limits<float>::infinity(),{}};
    match=FindReadyTransition(character,defaults,3,8);
    Check(match.row && !match.row->state.available && match.row->state.error=="unread state" &&
        match.row->transition_in.value==-2 && match.row->reverse_outro.value==std::numeric_limits<float>::infinity(),
        "lookup does not replace source fields with preview defaults");
    character.rows.insert(character.rows.begin(),ReadyTransitionInput{});
    match=FindReadyTransition(character,defaults,3,8);
    Check(match.resolution==ReadyTransitionResolution::UnreadKey && !match.row && match.position==0,
        "earlier unread key prevents a later first-match claim");
    character.rows={Edge(4,9,"BeforeUnread"),{}};
    Check(FindReadyTransition(character,defaults,3,8).row->state.value=="BeforeUnread","later unread key does not discard established match");
    character.rows.clear();character.enumerated=false;
    Check(FindReadyTransition(character,defaults,3,8).resolution==ReadyTransitionResolution::IncompleteTable,"unread table is not absent");
    character.enumerated=true;character.issues.push_back({0,"read","partial"});
    Check(FindReadyTransition(character,defaults,3,8).resolution==ReadyTransitionResolution::IncompleteTable,"partial table does not assert fallback");
    character.issues.clear();match=FindReadyTransition(character,defaults,3,8);
    Check(match.row && match.table==ReadyTransitionTable::OfficialDefault && match.row->state.value=="Fallback","qualified default fallback");
    defaults.insert(defaults.begin(),ReadyTransitionInput{});
    match=FindReadyTransition(character,defaults,3,8);
    Check(match.resolution==ReadyTransitionResolution::UnreadKey && match.table==ReadyTransitionTable::OfficialDefault,
        "default table also preserves unread keys");
    defaults.clear();
    Check(FindReadyTransition(character,defaults,3,8).resolution==ReadyTransitionResolution::Absent,"complete absent relation");
    Check(FindReadyTransition(character,defaults,-1,8).resolution==ReadyTransitionResolution::InvalidIndex &&
        FindReadyTransition(character,defaults,0,-1).resolution==ReadyTransitionResolution::InvalidIndex &&
        FindReadyTransition(character,defaults,std::numeric_limits<int>::max(),1).resolution==ReadyTransitionResolution::InvalidIndex &&
        FindReadyTransition(character,defaults,1,std::numeric_limits<int>::max()).resolution==ReadyTransitionResolution::InvalidIndex,
        "checked index conversion");

    AbilitySources graph;
    graph.skill_mappings.push_back({"setup",Path(1),{true,"ArbitrarySkill",{}},{true,"Caster",{}},{true,7,{}},{true,true,{}}});
    graph.skill_continuations.push_back({"setup",Path(7),{true,7,{}},{true,-1,{}},{true,true,{}}});
    graph.animations={Animation(0,"Before"),Animation(4,"UnrelatedPoseName"),Animation(8,"After")};
    auto sequences=FindPreparationSequences(graph);
    Check(sequences.size()==1 && sequences[0].mapping.skill.value=="ArbitrarySkill" &&
        sequences[0].animations.size()==1 && sequences[0].animations[0].state.value=="UnrelatedPoseName",
        "structural relation preserves source identity without names convention");
    graph.animations[1].state.value="Mutated";
    Check(sequences[0].animations[0].state.value=="UnrelatedPoseName","sequence owns source snapshots");
    auto nested=Animation(5,"Nested");nested.path.push_back({"Predicate","SuccessTaskList",ConfigSelection::Index,0,{}});
    graph.animations.push_back(nested);
    auto foreign=Animation(5,"OtherAbility");foreign.ability="other";graph.animations.push_back(foreign);
    Check(FindPreparationSequences(graph)[0].animations.size()==1,"no flattening or cross-ability links");
    auto later=graph.skill_mappings[0];later.path=Path(3);later.skill.value="LatestSlot";
    later.enabled={true,false,{}};later.target_alias={true,"Target",{}};
    graph.skill_mappings.push_back(later);
    graph.skill_continuations[0].child.value=2;graph.skill_continuations[0].enabled={};
    graph.animations[1].enabled={};graph.animations[1].target_alias={true,"Target",{}};
    sequences=FindPreparationSequences(graph);
    Check(sequences.size()==1 && sequences[0].mapping.skill.value=="LatestSlot" &&
        !sequences[0].mapping.enabled.value && sequences[0].mapping.target_alias.value=="Target" &&
        sequences[0].continuation.child.value==2 && !sequences[0].continuation.enabled.available &&
        !sequences[0].animations[0].enabled.available,"SDK reports declarations rather than preview eligibility");
    graph.skill_mappings.back().ability="other";
    Check(FindPreparationSequences(graph)[0].mapping.skill.value=="ArbitrarySkill","other ability cannot replace slot");
    graph.skill_continuations[0].control.value=9;
    Check(FindPreparationSequences(graph).empty(),"different control slot is not linked");
    graph.skill_continuations[0].control.value=7;
    graph.skill_continuations[0].path.push_back({"Predicate","SuccessTaskList",ConfigSelection::Index,0,{}});
    Check(FindPreparationSequences(graph).empty(),"nested continuation not treated as straight OnStart");
}
