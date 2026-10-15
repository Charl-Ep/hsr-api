#include "../../src/metadata/MetadataConnection.hpp"
#include <hksr/native/LoadedModule.hpp>
#include "RuntimeProfile.hpp"
#include <hksr/runtime/RuntimeThread.hpp>
#include "../../src/runtime/RuntimeAvailability.hpp"
#include "../../src/runtime/RuntimeInitialization.hpp"
#include <hksr/native/UnityMetadataLocator.hpp>
#include <Windows.h>
#include <cstring>
#include <stdexcept>
#include <mutex>

namespace hksr::sdk_v1::detail {
using namespace native;
namespace {
using Module = LoadedModule;
void** UnityTable(const Module& unity) {
    // Called by the periodic readiness check: locate once per loaded image,
    // not once per Present/poll. Caller enforces its qualified module pair.
    static std::mutex mutex;
    static unsigned char* located_base=nullptr;
    static void** cached=nullptr;
    std::lock_guard<std::mutex> lock(mutex);
    if(located_base==unity.base) return cached;
    located_base=unity.base;
    cached=nullptr;
    std::vector<std::vector<unsigned char>> storage;
    std::vector<MetadataCodeSection> code;
    for(const auto& section:unity.sections) {
        if(!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        storage.emplace_back(section.Misc.VirtualSize);
        auto& bytes=storage.back();
        if(!CopyMemoryChecked(bytes.data(),unity.base+section.VirtualAddress,bytes.size()))
            return nullptr;
        code.push_back({bytes.data(),bytes.size(),section.VirtualAddress});
    }
    const auto locations=LocateUnityMetadataTables(code,unity.size,[&](std::uint32_t rva,void* out,std::size_t size) {
        return rva<=unity.size && size<=unity.size-rva && CopyMemoryChecked(out,unity.base+rva,size);
    });
    for(const auto& location:locations) {
        // Multiple call sites using the same table are not a conflict.
        auto** table=reinterpret_cast<void**>(unity.base+location.table);
        if(cached && cached!=table) {cached=nullptr;break;}
        cached=table;
    }
    return cached;
}
void** ExportTable(const Module& game) {
    void* address = reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(game.base), "il2cpp_get_api_table"));
    if (address != game.base + 0x644BA0 || !game.Executable(address)) return nullptr;
    constexpr unsigned char expected[] = {0xE8,0x73,0x90,0x2C,0x18,0x7A,0x87,0x93,0x52,0x21,0x04,0x9E,0x67,0x2D,0x76,0x07};
    unsigned char actual[sizeof(expected)]{};
    if (!CopyMemoryChecked(actual, address, sizeof(actual)) || std::memcmp(expected, actual, sizeof(actual))) return nullptr;
    __try { return reinterpret_cast<void**(__fastcall*)()>(address)(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool BindTable(void** table, const Module& game, MetadataFunctions* result, ObjectFunctions* objects, std::vector<std::string>* errors) {
    *result = {};
    *objects = {};
    struct Slot { std::size_t index; const char* name; void** output; };
    Slot slots[] = {
        {63,"domain_get",&result->domain_get}, {65,"domain_get_assemblies",&result->domain_get_assemblies},
        {22,"assembly_get_image",&result->assembly_get_image}, {169,"image_class_count",&result->image_class_count},
        {170,"image_get_class",&result->image_get_class}, {37,"class_get_name",&result->class_get_name},
        {39,"class_get_namespace",&result->class_get_namespace}, {35,"class_get_methods",&result->class_get_methods},
        {117,"method_get_name",&result->method_get_name}, {116,"method_return_type",&result->method_return_type},
        {123,"method_param_count",&result->method_param_count}, {124,"method_get_param",&result->method_get_param},
        {161,"type_get_name",&result->type_get_name}, {13,"release_type_name",&result->release_type_name},
        {40,"class_get_parent",&result->class_get_parent}, {31,"class_get_fields",&result->class_get_fields},
        {73,"field_get_name",&result->field_get_name}, {76,"field_get_type",&result->field_get_type},
        {75,"field_get_offset",&result->field_get_offset},
        {72,"field_get_flags",&result->field_get_flags},
        {49,"class_from_type",&result->class_from_type},
        {147,"string_new",&objects->string_new}, {130,"object_new",&objects->object_new},
        // Both retained 4.5 and 4.6 Unity class-to-reflection wrappers use
        // slot51 followed by158. The former adapter's enum label50 was wrong.
        {51,"class_get_type",&objects->class_get_type}, {158,"type_get_object",&objects->type_get_object},
    };
    for (const auto& slot : slots) {
        void* entry = nullptr;
        if (!table || !CopyMemoryChecked(&entry, table + slot.index, sizeof(entry)) || !game.Executable(entry))
            errors->push_back(std::string(slot.name) + " is unavailable");
        else *slot.output = entry;
    }
    // Only enumeration operations are prerequisites of the type inventory.
    // Missing field access must not suppress unrelated method resolution.
    return result->domain_get && result->domain_get_assemblies && result->assembly_get_image &&
        result->image_class_count && result->image_get_class && result->class_get_name && result->class_get_namespace;
}
}
namespace {
MetadataConnection Connect(const Module& game,const Module& unity,bool allow_legacy_export) {
    MetadataFunctions functions{};
    ObjectFunctions objects{};
    std::vector<std::string> unity_errors, export_errors;
    // The retained successful session used Unity's populated table. Keep both
    // known sources, without making the obfuscated export a prerequisite.
    const bool unity_ready=BindTable(UnityTable(unity), game, &functions, &objects, &unity_errors);
    if (!unity_ready && !(allow_legacy_export && BindTable(ExportTable(game), game, &functions, &objects, &export_errors))) {
        std::string message=allow_legacy_export?"Neither known API table can enumerate the current domain.":"Qualified Unity API table cannot enumerate the current domain.";
        for(const auto& error:unity_errors)message+="\nUnity table: "+error;
        for(const auto& error:export_errors)message+="\nExport table: "+error;
        throw std::runtime_error(message);
    }
    // This layout is version-scoped evidence, NOT a cross-version promise.
    // The basic metadata service no longer depends on SpringManager probes.
    return {functions, {8, 42}, [game](void* address) { return game.Executable(address); }, unity_ready?unity_errors:export_errors, objects};
}
}
MetadataConnection ConnectRuntimeMetadata(RuntimeVersion version) {
    const auto& profile=RuntimeProfileFor(version);
    return Connect(GameModule(profile),UnityModule(profile),profile.legacy_export);
}
RuntimeThreadMethods ThreadConnection(const RuntimeProfile& profile) {
    const auto game=GameModule(profile);
    const auto unity=UnityModule(profile);
    auto read=[&](void** table){
        RuntimeThreadMethods out;
        struct Slot{std::size_t index;const char* name;void** result;};
        const Slot slots[]={{63,"domain_get",&out.domain},{153,"thread_current",&out.current},
            {154,"thread_attach",&out.attach},{155,"thread_detach",&out.detach}};
        for(const auto& slot:slots){
            void* value=nullptr;
            if(table && CopyMemoryChecked(&value,table+slot.index,sizeof(value)) && game.Executable(value))*slot.result=value;
            else out.binding_errors.push_back(std::string(slot.name)+" unavailable");
        }
        return out;
    };
    auto selected=read(UnityTable(unity));
    if(selected.current && selected.domain && selected.attach && selected.detach)return selected;
    if(profile.legacy_export) {
        auto fallback=read(ExportTable(game));
        if(fallback.current && fallback.domain && fallback.attach && fallback.detach)return fallback;
    }
    // Keep independently available Unity entries; borrowing an existing engine
    // attachment only needs current, not every metadata or allocator operation.
    return selected;
}
RuntimeAvailabilityResult Availability(const RuntimeProfile& profile) {
    if(!GetModuleHandleW(L"GameAssembly.dll") || !GetModuleHandleW(L"UnityPlayer.dll"))
        return {RuntimeAvailability::WaitingModules,{}};
    try {
        const auto game=GameModule(profile);
        const auto unity=UnityModule(profile);
        auto** table=UnityTable(unity);
        if(!table) return {RuntimeAvailability::Unsupported,"known Unity API table instruction is unavailable"};
        void* initialize=nullptr;
        if(!CopyMemoryChecked(&initialize,table,sizeof(initialize)))
            return {RuntimeAvailability::Unsupported,"runtime initialization entry cannot be read"};
        if(!initialize) return {RuntimeAvailability::WaitingTable,{}};
        if(!game.Executable(initialize)) return {RuntimeAvailability::Unsupported,"runtime initialization entry is not executable"};
        // Bounded reads (two wrapper calls), cached after the populated entry is
        // seen. Do not scan the 500MB image or invoke init/GC registration here.
        static std::mutex location_mutex;
        static void* located_entry=nullptr;
        static std::optional<RuntimeInitializationLocation> location;
        std::optional<RuntimeInitializationLocation> selected;
        {
            std::lock_guard<std::mutex> lock(location_mutex);
            if(located_entry!=initialize) {
                located_entry=initialize;
                const auto kernel=GetModuleHandleW(L"kernel32.dll");
                location=LocateRuntimeInitialization(static_cast<std::uint32_t>(static_cast<unsigned char*>(initialize)-game.base),game.size,
                    [&](std::uint32_t rva,void* out,std::size_t size) {
                        return rva<=game.size && size<=game.size-rva && CopyMemoryChecked(out,game.base+rva,size);
                    },reinterpret_cast<std::uintptr_t>(GetProcAddress(kernel,"EnterCriticalSection")),
                    reinterpret_cast<std::uintptr_t>(GetProcAddress(kernel,"LeaveCriticalSection")));
            }
            selected=location;
        }
        if(!selected) return {RuntimeAvailability::Unsupported,"official runtime initialization synchronization could not be resolved"};
        const auto initialized=ObserveInitialization(reinterpret_cast<CRITICAL_SECTION* const*>(game.base+selected->mutex_slot),
            reinterpret_cast<const int*>(game.base+selected->count));
        if(initialized!=RuntimeAvailability::Available)
            return {initialized,initialized==RuntimeAvailability::Unsupported?"official runtime initialization observation faulted":"waiting for official runtime initialization to complete"};
        const std::size_t slots[]={63,65,153,154,155};
        void* values[5]{};
        for(std::size_t i=0;i<5;++i) {
            if(!CopyMemoryChecked(&values[i],table+slots[i],sizeof(void*)))
                return {RuntimeAvailability::Unsupported,"Unity API table cannot be read"};
            if(!values[i]) return {RuntimeAvailability::WaitingTable,{}};
            if(!game.Executable(values[i])) return {RuntimeAvailability::Unsupported,"API table points outside known game code"};
        }
        return ObserveDomain(values[0],values[1]);
    } catch(const std::exception& e) {return {RuntimeAvailability::Unsupported,e.what()};}
}
}
namespace hksr::sdk_v1 {
RuntimeThreadMethods ResolveRuntimeThreadMethods(RuntimeVersion version){return detail::ThreadConnection(detail::RuntimeProfileFor(version));}
RuntimeAvailabilityResult ObserveRuntimeAvailability(RuntimeVersion version){
    try {return detail::Availability(detail::RuntimeProfileFor(version));}
    catch(const std::exception& error){return {RuntimeAvailability::Unsupported,error.what()};}
}
RuntimeAvailabilityResult ObserveRuntimeAvailability(){
    if(!GetModuleHandleW(L"GameAssembly.dll") || !GetModuleHandleW(L"UnityPlayer.dll"))
        return {RuntimeAvailability::WaitingModules,{}};
    try {return detail::Availability(detail::CurrentRuntimeProfile());}
    catch(const std::exception& e) {return {RuntimeAvailability::Unsupported,e.what()};}
}
}
