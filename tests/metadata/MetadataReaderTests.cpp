#include "../support/NativeMetadataFixture.hpp"
#include "../../src/metadata/MetadataTypeIndex.hpp"
#include "../../src/metadata/InstanceField.hpp"
#include "../../adapters/world/WorldFrameBindings.hpp"
#include <hksr/native/MetadataProvider.hpp>
#include <hksr/presentation/PresentationContext.hpp>
#include <hksr/runtime/RuntimeHost.hpp>
#include <hksr/presentation/PresentationBindings.hpp>
#include <algorithm>

using namespace hksr::test;
void TestNativeMetadataReader() {
    hksr::api::detail::MetadataTypeIndex index;
    index.Add("A", "Type", 7);
    index.Add("B", "Type", 11);
    Check(index.Find("A", "Type") == 7 && index.Find("B", "Type") == 11);
    bool missing = false, ambiguous = false;
    try {
        index.Find("C", "Type");
    } catch (const std::runtime_error&) {
        missing = true;
    }
    index.Add("A", "Type", 42);
    try {
        index.Find("A", "Type");
    } catch (const std::runtime_error&) {
        ambiguous = true;
    }
    Check(missing && ambiguous && index.Find("B", "Type") == 11);
    MetadataConnection connection{
        {Pointer(Domain),     Pointer(Assemblies), Pointer(Image),      Pointer(ClassCount),  Pointer(ClassAt),
         Pointer(ClassName),  Pointer(ClassSpace), Pointer(NextMethod), Pointer(Name),        Pointer(Returns),
         Pointer(ParamCount), Pointer(Param),      Pointer(TypeName),   Pointer(Release),     Pointer(Parent),
         Pointer(Field),      Pointer(FieldName),  Pointer(FieldType),  Pointer(FieldOffset), Pointer(ClassFromType),
         Pointer(FieldFlags)},
        {8, 42},
        [](void* code) { return code == &executable; },
        {}};
    NativeMetadataReader reader(connection);
    int class_a, class_b;
    classes = {&class_a, &class_a, &class_b};
    const auto types = reader.ReadTypes();
    Check(types.size() == 2 && types[0].type == &class_a && types[1].type == &class_b);
    // Same name in different runtime classes is preserved, not overwritten.
    Check(types[0].name == types[1].name);
    Check(reader.DerivesFrom(&class_a, &parent_class));
    Check(reader.DerivesFrom(&class_a, &class_a));
    Check(!reader.DerivesFrom(&class_a, &class_b)); // same name is not the same class
    parent_cycle = true;
    bool ancestry_failed = false;
    try {
        reader.DerivesFrom(&class_a, &class_b);
    } catch (const std::runtime_error&) {
        ancestry_failed = true;
    }
    Check(ancestry_failed);
    parent_cycle = false;
    FakeMethod bound{};
    bound.code = &executable;
    bound.flags = 0x16;
    bound.name = "Change";
    bound.result = "System.Boolean";
    bound.parameters = {"System.String", "System.String&"};
    FakeMethod abstract{};
    abstract.name = "Unbound";
    abstract.result = "System.Void";
    methods = {&bound, &abstract};
    const auto before_names = name_acquisitions;
    auto codes = reader.ReadCodeEntries(types[0]);
    Check(codes.codes == std::vector<void*>{&executable} && codes.errors.empty());
    Check(name_acquisitions == before_names); // no names/signatures expanded
    repeated_method = true;
    codes = reader.ReadCodeEntries(types[0]);
    Check(codes.codes.size() == 1 && !codes.errors.empty());
    repeated_method = false;
    auto unavailable_codes = connection;
    unavailable_codes.functions.class_get_methods = nullptr;
    Check(!NativeMetadataReader(unavailable_codes).ReadCodeEntries(types[0]).errors.empty());
    methods = {reinterpret_cast<FakeMethod*>(1), &bound};
    codes = reader.ReadCodeEntries(types[0]);
    Check(codes.errors.size() == 1 && codes.codes == std::vector<void*>{&executable});
    methods = {&bound, &abstract};
    parameter_class = &class_b;
    Check(reader.ParameterType({nullptr, &bound}, 0).type == &class_b);
    Check(reader.ReturnType({nullptr, &bound}).type == &class_b);
    parameter_class = &class_a;
    Check(reader.ParameterType({nullptr, &bound}, 0).type == &class_a);
    Check(reader.ReturnType({nullptr, &bound}).type == &class_a);
    bool absent_return = false;
    bound.result = nullptr;
    try {
        reader.ReturnType({nullptr, &bound});
    } catch (const std::runtime_error&) {
        absent_return = true;
    }
    Check(absent_return);
    bound.result = "System.Boolean";
    bool invalid_index = false;
    try {
        reader.ParameterType({nullptr, &bound}, 2);
    } catch (const std::out_of_range&) {
        invalid_index = true;
    }
    Check(invalid_index); // same display name doesn't substitute another class
    auto declarations = reader.ReadMethods(types[0]);
    Check(declarations.methods.size() == 2 && declarations.methods[0].is_static);
    Check(declarations.methods[0].entry.code == &executable && declarations.methods[0].entry.metadata == &bound);
    Check(!declarations.methods[1].entry.code);
    Check(name_acquisitions == 4 && name_acquisitions == name_releases);
    MetadataResolver resolver({declarations});
    Check(resolver
              .Resolve("Game", "Model", "Change", "System.Boolean", {"System.String", "System.String&"},
                       MethodKind::Static)
              .metadata == &bound);
    bound.parameters.assign(65, "System.Int32");
    Check(reader.ReadMethods(types[0]).methods[0].parameters.size() == 65);
    Check(name_acquisitions == name_releases);
    invalid_string = true;
    bool failed = false;
    try {
        reader.ReadMethods(types[0]);
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed && name_acquisitions == name_releases);
    invalid_string = false;
    repeated_method = true;
    failed = false;
    try {
        reader.ReadMethods(types[0]);
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed && name_acquisitions == name_releases);
    repeated_method = false;
    absent_domain = true;
    failed = false;
    try {
        reader.ReadTypes();
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed);
    absent_domain = false;
    const auto own_fields = reader.ReadFields(types[0], false);
    const auto all_fields = reader.ReadFields(types[0], true);
    Check(own_fields.size() == 1 && all_fields.size() == 2 && all_fields[1].name == "OnStart");
    Check(all_fields[1].owner_name == "Base" && all_fields[1].offset == 48);
    Check(all_fields[1].flags_observed && all_fields[1].flags == 6);
    base_field.flags = 0x16;
    Check(reader.ReadFields(types[0], true)[1].flags == 0x16);
    base_field.flags = 6;
    auto without_flags = connection;
    without_flags.functions.field_get_flags = nullptr;
    Check(!NativeMetadataReader(without_flags).ReadFields(types[0], true)[1].flags_observed);
    parent_cycle = true;
    failed = false;
    try {
        reader.ReadFields(types[0], true);
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed && name_acquisitions == name_releases);
    parent_cycle = false;
    auto without_fields = connection;
    without_fields.functions.class_get_fields = nullptr;
    NativeMetadataReader independent(without_fields);
    Check(independent.ReadTypes().size() == 2 && independent.ReadMethods(types[0]).methods.size() == 2);
    failed = false;
    try {
        independent.ReadFields(types[0], true);
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed);
    // A discovery query works without either MethodInfo layout or executable
    // predicate, and cannot return a callable Method or guessed static flag.
    auto discovery_only = connection;
    discovery_only.layout = {static_cast<std::size_t>(-1), static_cast<std::size_t>(-1)};
    discovery_only.executable = [](void*) -> bool { throw std::logic_error("discovery must not qualify code"); };
    NativeMetadataReader discovery(discovery_only);
    auto observations = discovery.ObserveMethods(types[0]);
    Check(observations.iteration_error.empty() && observations.methods.size() == 2);
    Check(observations.methods[0].name == "Change" && observations.methods[0].parameters.size() == 65);
    bound.name = nullptr;
    observations = discovery.ObserveMethods(types[0]);
    Check(observations.iteration_error.empty() && observations.methods.size() == 2);
    Check(!observations.methods[0].error.empty() && observations.methods[1].name == "Unbound" &&
          observations.methods[1].error.empty());
    bound.name = "Change";
    repeated_method = true;
    observations = discovery.ObserveMethods(types[0]);
    Check(observations.methods.size() == 1 && !observations.iteration_error.empty());
    repeated_method = false;
    auto no_iterator = discovery_only;
    no_iterator.functions.class_get_methods = nullptr;
    observations = NativeMetadataReader(no_iterator).ObserveMethods(types[0]);
    Check(observations.methods.empty() && !observations.iteration_error.empty());
    Check(name_acquisitions == name_releases);
    classes = {&class_a, nullptr, &class_b};
    const auto partial_types = discovery.ObserveTypes();
    Check(partial_types.types.size() == 2 && partial_types.errors.size() == 1);
    invalid_assembly_span = true;
    const auto invalid_inventory = discovery.ObserveTypes();
    Check(invalid_inventory.types.empty() && invalid_inventory.errors.size() == 1);
    invalid_assembly_span = false;
    classes = {&class_a, &class_b};
    Check(discovery.ParentType(types[0]).type == &parent_class);
    Check(!discovery.ParentType({"Game", "Base", &parent_class, nullptr}).type);
    Check(discovery.FieldType(&child_field).type == &class_a);
    auto field_rows = discovery.ObserveFields(types[0]);
    Check(field_rows.fields.size() == 1 && field_rows.fields[0].error.empty() &&
          field_rows.fields[0].declaration.flags_observed);
    child_field.name = nullptr;
    field_rows = discovery.ObserveFields(types[0]);
    Check(field_rows.fields.size() == 1 && !field_rows.fields[0].error.empty() && field_rows.iteration_error.empty());
    child_field.name = "Name";
    repeated_field = true;
    field_rows = discovery.ObserveFields(types[0]);
    Check(field_rows.fields.size() == 1 && !field_rows.iteration_error.empty());
    repeated_field = false;
    Check(!independent.ObserveFields(types[0]).iteration_error.empty());
    // Runtime use keeps independent valid declarations, while the explicit
    // NativeMetadataReader::ReadTypes/ReadMethods full-snapshot tools stay strict.
    classes = {&class_a, nullptr};
    bound.parameters = {"System.String"};
    FakeMethod broken{};
    broken.name = "Unrelated";
    broken.result = nullptr;
    methods = {&bound, &broken};
    auto partial_provider = native::MetadataProvider::Open(connection);
    // Capability failures remain queryable when no optional trace is supplied.
    // Other prepared capabilities are not gated by this aggregate diagnostic.
    {
        hksr::api::PresentationBindings bindings;
        // This fixture exposes no game types. Reference entries are still a
        // required construction contract; any unexpected retention fails cleanly.
        const auto retain = +[](void*, int, int, void*) -> int { return 0; };
        const auto target = +[](int, void*) -> void* { return nullptr; };
        const auto free = +[](int, void*) {};
        bindings.references = {{Pointer(retain), &class_a}, {Pointer(target), &class_a}, {Pointer(free), &class_a}};
        hksr::api::PresentationContext context(*partial_provider, bindings);
        context.Prepare(*partial_provider, {}, {}, {});
        const auto& errors = context.BindingErrors();
        Check(!errors.empty());
        const auto timing = std::find_if(errors.begin(), errors.end(), [](const auto& error) {
            return error.find("actor timing unavailable:") == 0;
        });
        Check(timing != errors.end() && timing->size() > std::string("actor timing unavailable:").size());
        Check(context.Close());
    }
    Check(partial_provider->TypeCount() == 1 && !partial_provider->UnavailableOperations().empty());
    auto partial = partial_provider->ReadType("Game", "Model");
    Check(partial.methods.size() == 1 && partial.method_issues.size() == 1);
    Check(partial.method_issues[0].declaration.name == "Unrelated");
    Check(partial_provider->Resolve("Game", "Model", "Change", "System.Boolean", {"System.String"}, MethodKind::Static)
              .metadata == &bound);
    bool bad_target = false;
    try {
        partial_provider->Resolve("Game", "Model", "Unrelated", "System.Void", {});
    } catch (const std::runtime_error&) {
        bad_target = true;
    }
    Check(bad_target && partial_provider->ReadFields("Game", "Model", false).size() == 1);
    // An unread overload with a known different parameter count cannot create
    // ambiguity for the healthy one-argument request.
    broken.name = "Change";
    broken.result = "System.Boolean";
    broken.parameters = {nullptr, nullptr};
    auto other_arity = native::MetadataProvider::Open(connection);
    Check(other_arity->Resolve("Game", "Model", "Change", "System.Boolean", {"System.String"}, MethodKind::Static)
              .metadata == &bound);
    // Same name/signature may be ambiguous. Preserve the failure in the value
    // snapshot so constructing another MetadataResolver cannot hide it.
    broken.parameters = {nullptr};
    auto matching_issue = native::MetadataProvider::Open(connection);
    MetadataResolver incomplete({matching_issue->ReadType("Game", "Model")});
    bad_target = false;
    try {
        incomplete.Resolve("Game", "Model", "Change", "System.Boolean", {"System.String"}, MethodKind::Static);
    } catch (const std::runtime_error&) {
        bad_target = true;
    }
    Check(bad_target && matching_issue->ReadFields("Game", "Model", false).size() == 1);
    // An unknown method name prevents proving uniqueness on this type only;
    // field access and the other known runtime classes remain usable.
    broken.name = nullptr;
    auto unknown_name = native::MetadataProvider::Open(connection);
    bad_target = false;
    try {
        unknown_name->Resolve("Game", "Model", "Change", "System.Boolean", {"System.String"}, MethodKind::Static);
    } catch (const std::runtime_error&) {
        bad_target = true;
    }
    Check(bad_target && unknown_name->ReadFields("Game", "Model", false).size() == 1);
    // Per-field binding is distinct from an explicitly complete field snapshot.
    // A broken unrelated declaration must not suppress a readable target.
    classes = {&class_a};
    methods = {&bound, &abstract};
    struct FakeObject {
        void* klass;
    } object{&class_a};
    FakeField broken_field{"Unrelated", nullptr, 32};
    field_inventory = {&child_field, &broken_field};
    auto field_provider = native::MetadataProvider::Open(connection);
    const auto selected_field = detail::ResolveInstanceField(*field_provider, &object, "Name", "System.String");
    Check(selected_field.offset == child_field.offset);
    const auto field_names_read = name_acquisitions;
    Check(detail::ResolveInstanceField(*field_provider, &object, "Name", "System.String").offset == child_field.offset);
    Check(name_acquisitions == field_names_read); // repeated per-frame reads reuse domain metadata
    bool full_fields_failed = false;
    try {
        field_provider->ReadFields("Game", "Model", false);
    } catch (const std::runtime_error&) {
        full_fields_failed = true;
    }
    Check(full_fields_failed); // a complete schema was requested, not a target
    // A same-name field whose type cannot be read may make the target ambiguous.
    broken_field.name = "Name";
    bad_target = false;
    auto same_name_field = native::MetadataProvider::Open(connection);
    try {
        detail::ResolveInstanceField(*same_name_field, &object, "Name", "System.String");
    } catch (const std::runtime_error&) {
        bad_target = true;
    }
    Check(bad_target);
    // Unknown names cannot be discarded to manufacture a unique field match.
    broken_field.name = nullptr;
    bad_target = false;
    auto unknown_field = native::MetadataProvider::Open(connection);
    try {
        detail::ResolveInstanceField(*unknown_field, &object, "Name", "System.String");
    } catch (const std::runtime_error&) {
        bad_target = true;
    }
    Check(bad_target);
    // GameWorld has a known declared-field anchor. Other unread fields and
    // optional flag diagnostics must not revoke this independent capability.
    auto world_connection = connection;
    world_connection.functions.class_get_name = Pointer(+[](void*) -> const char* { return "GameWorld"; });
    world_connection.functions.class_get_namespace = Pointer(+[](void*) -> const char* { return "RPG.GameCore"; });
    std::array<FakeMethod, 6> world_methods{};
    const std::array<const char*, 6> world_names = {"LateUpdate",      "get_EntityManagerRef",
                                                    "get_IsDisposing", "get_IsBattleGameWorld",
                                                    "get_IsPauseTick", "get_IsInTick"};
    methods.clear();
    for (std::size_t i = 0; i < world_methods.size(); ++i) {
        auto& method = world_methods[i];
        method.code = &executable;
        method.flags = 6;
        method.name = world_names[i];
        method.result = i == 0 ? "System.Void" : i == 1 ? "RPG.GameCore.EntityManager" : "System.Boolean";
        if (i == 0)
            method.parameters = {"System.Single"};
        methods.push_back(&method);
    }
    FakeField phase{"BindAdventurePhase", "RPG.Client.AdventurePhase", 72};
    FakeField unrelated{"Unrelated", nullptr, 32};
    auto read_world = [&] {
        auto provider = native::MetadataProvider::Open(world_connection);
        auto bindings = detail::ResolveWorldFrameBindings(*provider);
        Check(bindings.late_update.code && bindings.manager.code && bindings.disposing.code && bindings.battle.code &&
              bindings.paused.code && bindings.in_tick.code);
        return bindings;
    };
    field_inventory = {&phase, &unrelated};
    auto world = read_world();
    Check(world.has_phase_offset && world.phase_offset == 72 && !world.binding_errors.empty());
    unrelated.name = nullptr;
    Check(read_world().has_phase_offset); // an unread unrelated row does not erase the named anchor
    unrelated.name = "BindAdventurePhase";
    Check(!read_world().has_phase_offset); // a second named row with unread type is a real conflict
    unrelated.type = "RPG.Client.AdventurePhase";
    Check(!read_world().has_phase_offset);
    unrelated.type = "System.String";
    Check(read_world().has_phase_offset); // known other types cannot match the requested field
    field_inventory = {&unrelated};
    Check(!read_world().has_phase_offset);
    field_inventory = {&phase};
    failed_offset_field = &phase;
    Check(!read_world().has_phase_offset);
    failed_offset_field = nullptr;
    phase.flags = 0x16;
    Check(!read_world().has_phase_offset);
    phase.flags = 6;
    failed_field_flags = true;
    auto flags_provider = native::MetadataProvider::Open(world_connection);
    auto observed_phase = flags_provider->ObserveFields(flags_provider->FindType("RPG.GameCore", "GameWorld"));
    Check(observed_phase.fields.size() == 1 && observed_phase.fields[0].offset_observed &&
          !observed_phase.fields[0].declaration.flags_observed && !observed_phase.fields[0].error.empty());
    world = detail::ResolveWorldFrameBindings(*flags_provider);
    Check(world.has_phase_offset && world.phase_offset == 72 && !world.binding_errors.empty());
    full_fields_failed = false;
    try {
        flags_provider->ReadFields("RPG.GameCore", "GameWorld", false);
    } catch (const std::runtime_error&) {
        full_fields_failed = true;
    }
    Check(full_fields_failed); // a complete snapshot still requires every requested observation
    bad_target = false;
    try {
        detail::ResolveInstanceField(*flags_provider, &object, "BindAdventurePhase", "RPG.Client.AdventurePhase");
    } catch (const std::runtime_error&) {
        bad_target = true;
    }
    Check(bad_target); // the flag-dependent generic instance-field contract is unchanged
    failed_field_flags = false;
    auto no_flags_world = world_connection;
    no_flags_world.functions.field_get_flags = nullptr;
    auto no_flags_provider = native::MetadataProvider::Open(no_flags_world);
    Check(detail::ResolveWorldFrameBindings(*no_flags_provider).has_phase_offset);
    const auto original_child = child_field;
    child_field = phase;
    repeated_field = true;
    world = read_world();
    Check(world.has_phase_offset && !world.binding_errors.empty());
    repeated_field = false;
    child_field = original_child;
    field_inventory.clear();
    classes = {&class_a, &class_b};
    methods = {&bound, &abstract};
    // No modules in this test process: provider must not load/start the game.
    failed = false;
    try {
        RuntimeMetadata::Open();
    } catch (const std::exception&) {
        failed = true;
    }
    Check(failed && !GetModuleHandleW(L"GameAssembly.dll"));
    std::cout << "Native metadata producer tests passed (fake API table, not game acceptance).\n";
}
