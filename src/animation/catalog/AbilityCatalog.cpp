#include "CatalogReader.hpp"
#include "../../../adapters/animation/catalog/AbilitySchema.hpp"

namespace hksr::api {
struct OfficialCatalog::State::AbilityRead final : AbilitySourceRead {
    std::shared_ptr<State> owner;
    State& s;
    State::Scope scope;
    detail::AbilitySchema schema;
    AbilitySources output;
    CompletedAbilitySources completed;
    bool active_ability = false;
    void CompleteAbility() {
        if (!active_ability)
            return;
        completed.abilities.push_back(ability);
        auto append = [](auto& target, auto& source) {
            target.insert(target.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
        };
        append(completed.sources.animations, output.animations);
        append(completed.sources.waits, output.waits);
        append(completed.sources.removals, output.removals);
        append(completed.sources.references, output.references);
        append(completed.sources.skill_mappings, output.skill_mappings);
        append(completed.sources.skill_continuations, output.skill_continuations);
        append(completed.sources.issues, output.issues);
        completed.sources.nodes_read += output.nodes_read;
        output = {};
        active_ability = false;
    }
    struct Edge {
        ConfigStep step;
        void* object;
    };
    struct Node : detail::AbilityNodeInput {
        std::vector<Edge> edges;
    };
    std::map<void*, Node> cache;
    std::vector<std::string> abilities;
    std::size_t next_ability = 0;
    std::string ability;
    std::set<std::string> visited_abilities;
    std::set<void*> visited;
    struct Work {
        void* object;
        std::vector<ConfigStep> path;
    };
    std::vector<Work> pending;
    Method load{};
    bool initialized = false, draining = false, done = false, cancelled = false;
    std::optional<Work> current;
    Node building;
    std::vector<DeclaredField> fields;
    std::size_t next_field = 0;
    struct SequenceCursor {
        void* array = nullptr;
        void* dictionary = nullptr;
        Method lookup{};
        int index = 0, count = 0;
        std::string owner, field;
    } sequence;
    AbilityRead(std::shared_ptr<State> state, std::vector<std::string> names, TypeRelationships& relation)
        : owner(std::move(state)), s(*owner), scope(s), schema(s, relation, scope), abilities(std::move(names)) {}

    void Add(ConfigStep step, void* child) {
        if (child && schema.IsConfigObject(child)) {
            // Cached edges survive across frames even before the child is
            // visited. Retain that actual object, not just the source container.
            scope.Keep({{true, 0}, child});
            building.edges.push_back({std::move(step), child});
        }
    }
    void BeginField(const DeclaredField& field) {
        const auto owner_name = field.owner_namespace + "." + field.owner_name;
        try {
            Require(field.flags_observed, "field static/instance flags unavailable");
            if (field.flags & 0x10 || field.type == "System.String")
                return;
            const auto& ft = field.type;
            const bool array = ft.size() > 2 && ft.substr(ft.size() - 2) == "[]";
            const bool list = ft.compare(0, 32, "System.Collections.Generic.List<") == 0;
            const std::string dictionary = "System.Collections.Generic.Dictionary<System.String,";
            SequenceCursor next;
            next.owner = owner_name;
            next.field = field.name;
            if (array || list) {
                const auto element = array ? ft.substr(0, ft.size() - 2) : ft.substr(32, ft.size() - 33);
                if (element != "System.Object" && !schema.IsConfigType(element))
                    return;
                next.array = s.Sequence(current->object, fields, field.name.c_str(), scope);
                if (!next.array)
                    return;
                scope.Keep({{true, 0}, next.array});
                const auto count = s.arrays.Length(next.array);
                Require(count.read.returned && count.value >= 0, "sequence length unavailable");
                next.count = count.value;
                sequence = std::move(next);
            } else if (ft.compare(0, dictionary.size(), dictionary) == 0 && ft.back() == '>') {
                const auto value_type = ft.substr(dictionary.size(), ft.size() - dictionary.size() - 1);
                if (!schema.IsConfigType(value_type))
                    return; // prove reference-return ABI before get_Item
                next.dictionary = Read<void*>(current->object, field);
                if (!next.dictionary)
                    return;
                scope.Keep({{true, 0}, next.dictionary});
                const auto declaration = s.metadata.ReadObjectType(next.dictionary);
                auto keys = scope.Keep(s.Call(Getter(declaration, "get_Keys"), next.dictionary));
                const auto keys_type = s.metadata.ReadObjectType(keys);
                Require(native::Call(Instance(keys_type, "get_Count", "System.Int32"), &next.count, keys).returned &&
                            next.count >= 0,
                        "dictionary key count unavailable");
                auto object_type = scope.Keep(s.objects.ManagedType("System", "Object"));
                next.array = scope.Keep(s.arrays.Create(object_type, next.count));
                Require(native::CallVoid(Instance(keys_type, "System.Collections.ICollection.CopyTo", "System.Void",
                                                  {"System.Array", "System.Int32"}),
                                         keys, next.array, 0)
                            .returned,
                        "dictionary key copy failed");
                next.lookup = Instance(declaration, "get_Item", value_type.c_str(), {"System.String"});
                sequence = std::move(next);
            } else if (ft.compare(0, 27, "System.Collections.Generic.") == 0) {
                building.issues.push_back({owner_name + "." + field.name + ": unsupported container " + ft,
                                           AbilitySourceIssueKind::Unsupported});
            } else if (ft.compare(0, 7, "System.") != 0 && schema.IsConfigType(ft)) {
                Add({owner_name, field.name}, Read<void*>(current->object, field));
            }
        } catch (const std::exception& e) {
            building.issues.push_back(
                {owner_name + "." + field.name + ": " + e.what(), detail::AbilitySchema::IssueKind(e)});
        }
    }
    void ReadElement() {
        const auto i = sequence.index++;
        try {
            const auto item = s.arrays.Get(sequence.array, i);
            Require(item.call.returned, "sequence item unavailable");
            if (sequence.dictionary) {
                Require(item.object != nullptr, "dictionary key unavailable");
                const auto child = s.Call(sequence.lookup, sequence.dictionary, item.object);
                Require(child.call.returned, "dictionary value lookup failed");
                Add({sequence.owner, sequence.field, ConfigSelection::StringKey, -1, s.Text(item.object)},
                    child.object);
            } else
                Add({sequence.owner, sequence.field, ConfigSelection::Index, i, {}}, item.object);
        } catch (const std::exception& e) {
            building.issues.push_back(
                {sequence.owner + "." + sequence.field + "[" + std::to_string(i) + "]: " + e.what(),
                 detail::AbilitySchema::IssueKind(e)});
        }
    }
    void Emit(const Work& work, const Node& node) {
        if (node.animation) {
            auto entry = *node.animation;
            entry.ability = ability;
            entry.path = work.path;
            output.animations.push_back(std::move(entry));
        }
        if (node.wait) {
            auto entry = *node.wait;
            entry.ability = ability;
            entry.path = work.path;
            output.waits.push_back(std::move(entry));
        }
        if (node.removal) {
            auto entry = *node.removal;
            entry.ability = ability;
            entry.path = work.path;
            output.removals.push_back(std::move(entry));
        }
        if (node.reference) {
            auto entry = *node.reference;
            entry.ability = ability;
            entry.path = work.path;
            output.references.push_back(std::move(entry));
        }
        if (node.skill_mapping) {
            auto entry = *node.skill_mapping;
            entry.ability = ability;
            entry.path = work.path;
            output.skill_mappings.push_back(std::move(entry));
        }
        if (node.skill_continuation) {
            auto entry = *node.skill_continuation;
            entry.ability = ability;
            entry.path = work.path;
            output.skill_continuations.push_back(std::move(entry));
        }
        // Preserve repeated ordered slots while expanding a shared/cyclic node
        // once per root. Cache lifetime covers ALL roots, not just one frame.
        if (!visited.insert(work.object).second)
            return;
        for (const auto& issue : node.issues)
            output.issues.push_back({ability, issue.error, work.path, issue.kind});
        for (auto edge = node.edges.rbegin(); edge != node.edges.rend(); ++edge) {
            auto path = work.path;
            path.push_back(edge->step);
            pending.push_back({edge->object, std::move(path)});
        }
    }
    void Step() {
        if (draining) {
            if (!scope.ids.empty())
                scope.ReleaseOne();
            else {
                done = true;
                if (cancelled) {
                    output = {};
                    completed = {};
                }
            }
            return;
        }
        if (!initialized) {
            initialized = true;
            try {
                if (!abilities.empty())
                    load = schema.LoadMethod();
            } catch (const std::exception& e) {
                completed.sources.issues.push_back({{}, e.what(), {}});
                std::set<std::string> unique(abilities.begin(), abilities.end());
                completed.abilities.assign(unique.begin(), unique.end());
                draining = true;
            }
            return;
        }
        if (current) {
            if (sequence.index < sequence.count) {
                ReadElement();
                return;
            }
            if (next_field < fields.size()) {
                BeginField(fields[next_field++]);
                return;
            }
            auto found = cache.emplace(current->object, std::move(building)).first;
            ++output.nodes_read;
            Emit(*current, found->second);
            current.reset();
            fields.clear();
            return;
        }
        if (!pending.empty()) {
            auto work = std::move(pending.back());
            pending.pop_back();
            const auto found = cache.find(work.object);
            if (found != cache.end())
                Emit(work, found->second);
            else {
                current = std::move(work);
                building = {};
                fields.clear();
                next_field = 0;
                sequence = {};
                static_cast<detail::AbilityNodeInput&>(building) = schema.ReadNode(current->object, fields);
            }
            return;
        }
        CompleteAbility();
        if (next_ability == abilities.size()) {
            draining = true;
            return;
        }
        ability = abilities[next_ability++];
        if (!visited_abilities.insert(ability).second)
            return;
        active_ability = true;
        visited.clear();
        try {
            Require(!ability.empty(), "ability key empty");
            auto input = scope.Keep(s.objects.NewStringUtf8(ability.c_str()));
            auto root = scope.Keep(s.Call(load, input));
            pending.push_back({root, {}});
        } catch (const std::exception& e) {
            output.issues.push_back({ability, e.what(), {}});
        }
    }
    void Advance(std::uint32_t budget_microseconds) override {
        const auto start = std::chrono::steady_clock::now();
        // Even a zero budget makes one unit of progress (also deterministic for
        // tests). Expiry pauses; it never rejects or truncates the graph.
        if (done)
            return;
        do {
            Step();
        } while (!done && std::chrono::steady_clock::now() - start < std::chrono::microseconds(budget_microseconds));
    }
    void Cancel() override {
        cancelled = true;
        draining = true;
        output = {};
        completed = {};
    }
    bool Done() const override {
        return done;
    }
    AbilitySources Take() override {
        Require(done, "ability discovery still in progress");
        auto result = std::move(completed.sources);
        completed = {};
        return result;
    }
    CompletedAbilitySources DrainCompleted() override {
        auto result = std::move(completed);
        completed = {};
        return result;
    }
};
std::unique_ptr<AbilitySourceRead> OfficialCatalog::BeginAbilitySources(std::vector<std::string> abilities,
                                                                        TypeRelationships& types) {
    return std::make_unique<State::AbilityRead>(*owner_, std::move(abilities), types);
}
AbilitySources OfficialCatalog::ReadAbilitySources(const std::vector<std::string>& abilities,
                                                   TypeRelationships& types) {
    auto read = BeginAbilitySources(abilities, types);
    while (!read->Done())
        read->Advance(2000);
    return read->Take();
}
}
