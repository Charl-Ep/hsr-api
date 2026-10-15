#include "../support/NativeMetadataFixture.hpp"
#include "../../src/metadata/MetadataTypeIndex.hpp"

using namespace hksr::test;
void TestNativeMetadataReader() {
    hksr::api::detail::MetadataTypeIndex index;
    index.Add("A","Type",7); index.Add("B","Type",11);
    Check(index.Find("A","Type")==7 && index.Find("B","Type")==11);
    bool missing=false,ambiguous=false;
    try{index.Find("C","Type");}catch(const std::runtime_error&){missing=true;}
    index.Add("A","Type",42);
    try{index.Find("A","Type");}catch(const std::runtime_error&){ambiguous=true;}
    Check(missing && ambiguous && index.Find("B","Type")==11);
    MetadataConnection connection{
        {Pointer(Domain), Pointer(Assemblies), Pointer(Image), Pointer(ClassCount), Pointer(ClassAt),
         Pointer(ClassName), Pointer(ClassSpace), Pointer(NextMethod), Pointer(Name), Pointer(Returns),
         Pointer(ParamCount), Pointer(Param), Pointer(TypeName), Pointer(Release), Pointer(Parent),
         Pointer(Field), Pointer(FieldName), Pointer(FieldType), Pointer(FieldOffset), Pointer(ClassFromType),Pointer(FieldFlags)}, {8,42},
        [](void* code) { return code == &executable; }, {}
    };
    NativeMetadataReader reader(connection);
    int class_a, class_b;
    classes = {&class_a, &class_a, &class_b};
    const auto types = reader.ReadTypes();
    Check(types.size() == 2 && types[0].type == &class_a && types[1].type == &class_b);
    // Same name in different runtime classes is preserved, not overwritten.
    Check(types[0].name == types[1].name);
    Check(reader.DerivesFrom(&class_a,&parent_class));
    Check(reader.DerivesFrom(&class_a,&class_a));
    Check(!reader.DerivesFrom(&class_a,&class_b)); // same name is not the same class
    parent_cycle=true;
    bool ancestry_failed=false;
    try { reader.DerivesFrom(&class_a,&class_b); } catch(const std::runtime_error&) { ancestry_failed=true; }
    Check(ancestry_failed); parent_cycle=false;
    FakeMethod bound{};
    bound.code=&executable; bound.flags=0x16; bound.name="Change"; bound.result="System.Boolean";
    bound.parameters={"System.String", "System.String&"};
    FakeMethod abstract{};
    abstract.name="Unbound"; abstract.result="System.Void";
    methods={&bound, &abstract};
    const auto before_names=name_acquisitions;
    auto codes=reader.ReadCodeEntries(types[0]);
    Check(codes.codes==std::vector<void*>{&executable} && codes.errors.empty());
    Check(name_acquisitions==before_names); // no names/signatures expanded
    repeated_method=true;codes=reader.ReadCodeEntries(types[0]);
    Check(codes.codes.size()==1 && !codes.errors.empty());repeated_method=false;
    auto unavailable_codes=connection;unavailable_codes.functions.class_get_methods=nullptr;
    Check(!NativeMetadataReader(unavailable_codes).ReadCodeEntries(types[0]).errors.empty());
    methods={reinterpret_cast<FakeMethod*>(1),&bound};
    codes=reader.ReadCodeEntries(types[0]);
    Check(codes.errors.size()==1 && codes.codes==std::vector<void*>{&executable});
    methods={&bound,&abstract};
    parameter_class = &class_b;
    Check(reader.ParameterType({nullptr, &bound}, 0).type == &class_b);
    Check(reader.ReturnType({nullptr,&bound}).type==&class_b);
    parameter_class = &class_a;
    Check(reader.ParameterType({nullptr, &bound}, 0).type == &class_a);
    Check(reader.ReturnType({nullptr,&bound}).type==&class_a);
    bool absent_return=false; bound.result=nullptr;
    try { reader.ReturnType({nullptr,&bound}); } catch(const std::runtime_error&) { absent_return=true; }
    Check(absent_return); bound.result="System.Boolean";
    bool invalid_index = false;
    try { reader.ParameterType({nullptr, &bound}, 2); } catch (const std::out_of_range&) { invalid_index = true; }
    Check(invalid_index); // same display name doesn't substitute another class
    auto declarations = reader.ReadMethods(types[0]);
    Check(declarations.methods.size()==2 && declarations.methods[0].is_static);
    Check(declarations.methods[0].entry.code==&executable && declarations.methods[0].entry.metadata==&bound);
    Check(!declarations.methods[1].entry.code);
    Check(name_acquisitions==4 && name_acquisitions==name_releases);
    MetadataResolver resolver({declarations});
    Check(resolver.Resolve("Game","Model","Change","System.Boolean",{"System.String","System.String&"},MethodKind::Static).metadata==&bound);
    bound.parameters.assign(65, "System.Int32");
    Check(reader.ReadMethods(types[0]).methods[0].parameters.size()==65);
    Check(name_acquisitions==name_releases);
    invalid_string=true;
    bool failed=false;
    try { reader.ReadMethods(types[0]); } catch (const std::exception&) { failed=true; }
    Check(failed && name_acquisitions==name_releases);
    invalid_string=false; repeated_method=true; failed=false;
    try { reader.ReadMethods(types[0]); } catch (const std::exception&) { failed=true; }
    Check(failed && name_acquisitions==name_releases);
    repeated_method=false; absent_domain=true; failed=false;
    try { reader.ReadTypes(); } catch (const std::exception&) { failed=true; }
    Check(failed); absent_domain=false;
    const auto own_fields = reader.ReadFields(types[0], false);
    const auto all_fields = reader.ReadFields(types[0], true);
    Check(own_fields.size()==1 && all_fields.size()==2 && all_fields[1].name=="OnStart");
    Check(all_fields[1].owner_name=="Base" && all_fields[1].offset==48);
    Check(all_fields[1].flags_observed && all_fields[1].flags==6);
    base_field.flags=0x16;
    Check(reader.ReadFields(types[0],true)[1].flags==0x16); base_field.flags=6;
    auto without_flags=connection; without_flags.functions.field_get_flags=nullptr;
    Check(!NativeMetadataReader(without_flags).ReadFields(types[0],true)[1].flags_observed);
    parent_cycle=true; failed=false;
    try { reader.ReadFields(types[0], true); } catch (const std::exception&) { failed=true; }
    Check(failed && name_acquisitions==name_releases); parent_cycle=false;
    auto without_fields=connection;
    without_fields.functions.class_get_fields=nullptr;
    NativeMetadataReader independent(without_fields);
    Check(independent.ReadTypes().size()==2 && independent.ReadMethods(types[0]).methods.size()==2);
    failed=false;
    try{independent.ReadFields(types[0],true);}catch(const std::exception&){failed=true;}
    Check(failed);
    // A discovery query works without either MethodInfo layout or executable
    // predicate, and cannot return a callable Method or guessed static flag.
    auto discovery_only=connection;
    discovery_only.layout={static_cast<std::size_t>(-1),static_cast<std::size_t>(-1)};
    discovery_only.executable=[](void*)->bool {throw std::logic_error("discovery must not qualify code");};
    NativeMetadataReader discovery(discovery_only);
    auto observations=discovery.ObserveMethods(types[0]);
    Check(observations.iteration_error.empty() && observations.methods.size()==2);
    Check(observations.methods[0].name=="Change" && observations.methods[0].parameters.size()==65);
    bound.name=nullptr;
    observations=discovery.ObserveMethods(types[0]);
    Check(observations.iteration_error.empty() && observations.methods.size()==2);
    Check(!observations.methods[0].error.empty() && observations.methods[1].name=="Unbound" && observations.methods[1].error.empty());
    bound.name="Change";repeated_method=true;
    observations=discovery.ObserveMethods(types[0]);
    Check(observations.methods.size()==1 && !observations.iteration_error.empty());
    repeated_method=false;
    auto no_iterator=discovery_only;no_iterator.functions.class_get_methods=nullptr;
    observations=NativeMetadataReader(no_iterator).ObserveMethods(types[0]);
    Check(observations.methods.empty() && !observations.iteration_error.empty());
    Check(name_acquisitions==name_releases);
    classes={&class_a,nullptr,&class_b};
    const auto partial_types=discovery.ObserveTypes();
    Check(partial_types.types.size()==2 && partial_types.errors.size()==1);
    invalid_assembly_span=true;
    const auto invalid_inventory=discovery.ObserveTypes();
    Check(invalid_inventory.types.empty() && invalid_inventory.errors.size()==1);
    invalid_assembly_span=false;
    classes={&class_a,&class_b};
    Check(discovery.ParentType(types[0]).type==&parent_class);
    Check(!discovery.ParentType({"Game","Base",&parent_class,nullptr}).type);
    Check(discovery.FieldType(&child_field).type==&class_a);
    auto field_rows=discovery.ObserveFields(types[0]);
    Check(field_rows.fields.size()==1 && field_rows.fields[0].error.empty() && field_rows.fields[0].declaration.flags_observed);
    child_field.name=nullptr;
    field_rows=discovery.ObserveFields(types[0]);
    Check(field_rows.fields.size()==1 && !field_rows.fields[0].error.empty() && field_rows.iteration_error.empty());
    child_field.name="Name";repeated_field=true;
    field_rows=discovery.ObserveFields(types[0]);
    Check(field_rows.fields.size()==1 && !field_rows.iteration_error.empty());
    repeated_field=false;
    Check(!independent.ObserveFields(types[0]).iteration_error.empty());
    // No modules in this test process: provider must not load/start the game.
    failed=false;
    try { RuntimeMetadata::Open(hksr::api::RuntimeVersion::Hsr450); } catch (const std::exception&) { failed=true; }
    Check(failed && !GetModuleHandleW(L"GameAssembly.dll"));
    std::cout << "Native metadata producer tests passed (fake API table, not game acceptance).\n";
}
