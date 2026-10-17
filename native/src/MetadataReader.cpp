#include <hksr/native/MetadataReader.hpp>
#include <Windows.h>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <unordered_set>
#include <limits>

namespace hksr::sdk_v1::native {
bool CopyMemoryChecked(void* destination, const void* source, std::size_t size) {
    __try { std::memcpy(destination, source, size); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
std::string CopyNativeString(const char* source) {
    if (!source) throw std::runtime_error("metadata returned a null string");
    std::string result;
    // Names are NUL-terminated API results. Read within each committed region;
    // no arbitrary name-length cap that silently truncates valid declarations.
    while (true) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(source, &info, sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            throw std::runtime_error("metadata string is not readable");
        const auto count = reinterpret_cast<const char*>(info.BaseAddress) + info.RegionSize - source;
        for (std::ptrdiff_t i = 0; i < count; ++i) {
            char byte = 0;
            if (!CopyMemoryChecked(&byte, source + i, 1))
                throw std::runtime_error("metadata string read fault");
            if (!byte) return result;
            result.push_back(byte);
        }
        source += count;
    }
}

template<class R, class... A>
static bool Invoke(void* address, R* output, A... args) {
    if (!address) return false;
    __try {
        *output = reinterpret_cast<R(__fastcall*)(A...)>(address)(args...);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class R, class... A>
static R Read(void* address, const char* operation, A... args) {
    R result{};
    if (!Invoke(address, &result, args...)) throw std::runtime_error(operation);
    return result;
}
static bool ReleaseName(void* address, void* name) {
    __try {
        reinterpret_cast<void(__fastcall*)(void*)>(address)(name);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
NativeMetadataReader::NativeMetadataReader(MetadataConnection connection)
    : connection_(std::move(connection)) {}

bool NativeMetadataReader::DerivesFrom(void* klass,void* base) const {
    if(!klass || !base) throw std::invalid_argument("ancestry needs two current classes");
    std::unordered_set<void*> visited;
    while(klass) {
        if(!visited.insert(klass).second) throw std::runtime_error("metadata ancestry cycle");
        if(klass==base) return true;
        klass=Read<void*>(connection_.functions.class_get_parent,"class_get_parent failed",klass,static_cast<void*>(nullptr));
    }
    return false;
}

NativeType NativeMetadataReader::DescribeType(void* klass) const {
    if (!klass) throw std::invalid_argument("cannot describe null runtime class");
    const auto& api = connection_.functions;
    return {CopyNativeString(Read<const char*>(api.class_get_namespace, "class_get_namespace failed", klass)),
        CopyNativeString(Read<const char*>(api.class_get_name, "class_get_name failed", klass)), klass, nullptr};
}
NativeType NativeMetadataReader::ParameterType(Method method, unsigned index) const {
    if (!method.metadata) throw std::invalid_argument("parameter type requires current MethodInfo");
    const auto& api = connection_.functions;
    const auto count = Read<unsigned>(api.method_param_count, "method_param_count failed", method.metadata);
    if (index >= count) throw std::out_of_range("method parameter index");
    void* type = Read<void*>(api.method_get_param, "method_get_param failed", method.metadata, index);
    if (!type) throw std::runtime_error("declared parameter type is null");
    return DescribeType(Read<void*>(api.class_from_type, "class_from_type failed", type));
}
NativeType NativeMetadataReader::ReturnType(Method method) const {
    if(!method.metadata) throw std::invalid_argument("return type requires current MethodInfo");
    const auto& api=connection_.functions;
    void* type=Read<void*>(api.method_return_type,"method_return_type failed",method.metadata);
    if(!type) throw std::runtime_error("declared return type is null");
    return DescribeType(Read<void*>(api.class_from_type,"class_from_type failed",type));
}

std::string NativeMetadataReader::TypeName(void* type) const {
    const auto& api = connection_.functions;
    if (!type || !api.release_type_name)
        throw std::runtime_error("type name or its paired release entry is unavailable");
    auto* buffer = Read<void*>(api.type_get_name, "type_get_name failed", type);
    // Release once even if copying fails. A release fault is outcome-unknown;
    // never call it a second time to make diagnostics look successful.
    std::string result;
    try { result = CopyNativeString(static_cast<const char*>(buffer)); }
    catch (...) { if (buffer) ReleaseName(api.release_type_name, buffer); throw; }
    if (buffer && !ReleaseName(api.release_type_name, buffer))
        throw std::runtime_error("type-name release failed; outcome unknown");
    return result;
}

TypeObservations NativeMetadataReader::ObserveTypes() const {
    const auto& api = connection_.functions;
    TypeObservations result;
    try {
    void* domain = Read<void*>(api.domain_get, "domain_get failed");
    if (!domain) throw std::runtime_error("engine domain is not available yet");
    std::size_t assembly_count = 0;
    auto** assemblies = Read<void**>(api.domain_get_assemblies, "domain_get_assemblies failed", domain, &assembly_count);
    // Validate the returned span, not an arbitrary count ceiling. A bad native
    // count must not become millions of identical per-element error records.
    auto at=reinterpret_cast<std::uintptr_t>(assemblies);
    if(assembly_count>(std::numeric_limits<std::size_t>::max)()/sizeof(void*))
        throw std::runtime_error("assembly count byte size overflows");
    auto remaining=assembly_count*sizeof(void*);
    if(remaining>(std::numeric_limits<std::uintptr_t>::max)()-at)
        throw std::runtime_error("assembly list address overflows");
    while(remaining) {
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(at),&info,sizeof(info)) || info.State!=MEM_COMMIT ||
            (info.Protect&(PAGE_NOACCESS|PAGE_GUARD))) throw std::runtime_error("assembly list span is not readable");
        const auto available=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize-at;
        const auto part=remaining<available?remaining:available;
        if(!part) throw std::runtime_error("assembly list span cannot advance");
        at+=part;remaining-=part;
    }
    std::unordered_set<void*> seen;
    for (std::size_t a = 0; a < assembly_count; ++a) {
        try {
        void* assembly = nullptr;
        if (!assemblies || !CopyMemoryChecked(&assembly, assemblies + a, sizeof(assembly)))
            throw std::runtime_error("assembly list read failed");
        void* image = Read<void*>(api.assembly_get_image, "assembly_get_image failed", assembly);
        const auto count = Read<std::size_t>(api.image_class_count, "image_class_count failed", image);
        for (std::size_t i = 0; i < count; ++i) {
            try {
            void* type = Read<void*>(api.image_get_class, "image_get_class failed", image, i);
            if (!type) throw std::runtime_error("image_get_class returned null within its declared count");
            if (!seen.insert(type).second) continue;
            const auto* name = Read<const char*>(api.class_get_name, "class_get_name failed", type);
            const auto* space = Read<const char*>(api.class_get_namespace, "class_get_namespace failed", type);
            result.types.push_back({CopyNativeString(space), CopyNativeString(name), type, image});
            } catch(const std::exception& e) {
                result.errors.push_back("assembly["+std::to_string(a)+"].class["+std::to_string(i)+"]: "+e.what());
            }
        }
        } catch(const std::exception& e) {
            result.errors.push_back("assembly["+std::to_string(a)+"]: "+e.what());
        }
    }
    } catch(const std::exception& e) {result.errors.push_back(e.what());}
    return result;
}
std::vector<NativeType> NativeMetadataReader::ReadTypes() const {
    auto result=ObserveTypes();
    if(!result.errors.empty()) throw std::runtime_error(result.errors.front());
    return std::move(result.types);
}

NativeType NativeMetadataReader::ParentType(const NativeType& type) const {
    auto* parent=Read<void*>(connection_.functions.class_get_parent,"class_get_parent failed",type.type,static_cast<void*>(nullptr));
    return parent?DescribeType(parent):NativeType{};
}
NativeType NativeMetadataReader::FieldType(void* field) const {
    auto* type=Read<void*>(connection_.functions.field_get_type,"field_get_type failed",field);
    if(!type) throw std::runtime_error("declared field type is null");
    return DescribeType(Read<void*>(connection_.functions.class_from_type,"class_from_type failed",type));
}
NativeType NativeMetadataReader::FromTypeDescriptor(void* descriptor) const {
    if(!descriptor)throw std::invalid_argument("null IL2CPP type descriptor");
    return DescribeType(Read<void*>(connection_.functions.class_from_type,"class_from_type failed",descriptor));
}
FieldObservations NativeMetadataReader::ObserveFields(const NativeType& type) const {
    FieldObservations result;
    const auto& api=connection_.functions;
    void* iterator=nullptr;
    std::unordered_set<void*> seen;
    while(true) {
        void* field=nullptr;
        try {field=Read<void*>(api.class_get_fields,"class_get_fields failed",type.type,&iterator);}
        catch(const std::exception& e) {result.iteration_error=e.what();break;}
        if(!field) break;
        if(!seen.insert(field).second) {result.iteration_error="field iterator repeated a field";break;}
        FieldObservation row;row.metadata=field;
        auto& out=row.declaration;out.owner_namespace=type.name_space;out.owner_name=type.name;
        try {
            out.name=CopyNativeString(Read<const char*>(api.field_get_name,"field_get_name failed",field));
            out.type=TypeName(Read<void*>(api.field_get_type,"field_get_type failed",field));
            out.offset=Read<std::size_t>(api.field_get_offset,"field_get_offset failed",field);
            if(api.field_get_flags) {
                out.flags=Read<std::uint32_t>(api.field_get_flags,"field_get_flags failed",field);
                out.flags_observed=true;
            }
        } catch(const std::exception& e) {row.error=e.what();}
        result.fields.push_back(std::move(row));
    }
    return result;
}

MethodObservations NativeMetadataReader::ObserveMethods(const NativeType& type) const {
    const auto& api = connection_.functions;
    MethodObservations result;
    void* iterator = nullptr;
    std::unordered_set<void*> seen;
    while (true) {
        void* method=nullptr;
        try {method=Read<void*>(api.class_get_methods, "class_get_methods failed", type.type, &iterator);}
        catch(const std::exception& e) {result.iteration_error=e.what();break;}
        if (!method) break;
        if (!seen.insert(method).second) {result.iteration_error="method iterator repeated a method";break;}
        MethodObservation row;row.metadata=method;
        try {
            row.name=CopyNativeString(Read<const char*>(api.method_get_name,"method_get_name failed",method));
            row.result=TypeName(Read<void*>(api.method_return_type,"method_return_type failed",method));
            const auto count=Read<unsigned>(api.method_param_count,"method_param_count failed",method);
            for(unsigned i=0;i<count;++i)
                row.parameters.push_back(TypeName(Read<void*>(api.method_get_param,"method_get_param failed",method,i)));
        } catch(const std::exception& e) {row.error=e.what();}
        result.methods.push_back(std::move(row));
    }
    return result;
}

RuntimeCodeEntries NativeMetadataReader::ReadCodeEntries(const NativeType& type) const {
    RuntimeCodeEntries result;
    void* iterator=nullptr;
    std::unordered_set<void*> seen;
    while(true) {
        void* method=nullptr;
        try {method=Read<void*>(connection_.functions.class_get_methods,"class_get_methods failed",type.type,&iterator);}
        catch(const std::exception& e){result.errors.push_back(e.what());break;}
        if(!method)break;
        if(!seen.insert(method).second){result.errors.push_back("method iterator repeated a method");break;}
        void* code=nullptr;
        if(!CopyMemoryChecked(&code,static_cast<const unsigned char*>(method)+connection_.layout.method_code_offset,sizeof(code))) {
            result.errors.push_back("MethodInfo code read failed at method index "+std::to_string(seen.size()-1));
            continue;
        }
        if(code)result.codes.push_back(code);
    }
    return result;
}

RuntimeMethods NativeMetadataReader::ObserveDeclaredMethods(const NativeType& type) const {
    const auto observed=ObserveMethods(type);
    RuntimeMethods result;result.iteration_error=observed.iteration_error;
    for(const auto& row:observed.methods) {
        RuntimeMethodObservation item;auto& declaration=item.declaration;
        declaration.name=row.name;declaration.result=row.result;declaration.parameters=row.parameters;
        declaration.entry.metadata=row.metadata;
        item.error=row.error;
        try {
        if(!item.error.empty())throw std::runtime_error(item.error);
        void* method=row.metadata;
        std::uint16_t flags = 0;
        void* code = nullptr;
        const auto* bytes = static_cast<const unsigned char*>(method);
        if (!CopyMemoryChecked(&flags, bytes + connection_.layout.method_flags_offset, sizeof(flags)) ||
            !CopyMemoryChecked(&code, bytes + connection_.layout.method_code_offset, sizeof(code)))
            throw std::runtime_error("MethodInfo layout read failed");
        declaration.is_static = (flags & 0x10) != 0;
        // Abstract/unbound entries still belong in the declaration list. Their
        // existence must not prevent a different method on this type resolving.
        declaration.entry = {code && connection_.executable(code) ? code : nullptr, method};
        }catch(const std::exception& e){item.error=e.what();}
        result.methods.push_back(std::move(item));
    }
    return result;
}
DeclaredType NativeMetadataReader::ReadMethods(const NativeType& type) const {
    const auto observed=ObserveDeclaredMethods(type);
    if(!observed.iteration_error.empty())throw std::runtime_error(observed.iteration_error);
    DeclaredType result{type.name_space,type.name,{}};
    for(const auto& row:observed.methods) {
        if(!row.error.empty())throw std::runtime_error(row.error);
        result.methods.push_back(row.declaration);
    }
    return result;
}

std::vector<DeclaredField> NativeMetadataReader::ReadFields(const NativeType& type, bool include_parents) const {
    const auto& api = connection_.functions;
    std::vector<DeclaredField> fields;
    std::unordered_set<void*> classes;
    void* current = type.type;
    while (current) {
        if (!classes.insert(current).second) throw std::runtime_error("metadata parent chain contains a cycle");
        const auto space = CopyNativeString(Read<const char*>(api.class_get_namespace, "class_get_namespace failed", current));
        const auto owner = CopyNativeString(Read<const char*>(api.class_get_name, "class_get_name failed", current));
        void* iterator = nullptr;
        std::unordered_set<void*> seen;
        while (true) {
            auto* field = Read<void*>(api.class_get_fields, "class_get_fields failed", current, &iterator);
            if (!field) break;
            if (!seen.insert(field).second) throw std::runtime_error("field iterator repeated a field");
            const auto name = CopyNativeString(Read<const char*>(api.field_get_name, "field_get_name failed", field));
            const auto field_type = TypeName(Read<void*>(api.field_get_type, "field_get_type failed", field));
            const auto offset = Read<std::size_t>(api.field_get_offset, "field_get_offset failed", field);
            DeclaredField declaration{space, owner, name, field_type, offset};
            if(api.field_get_flags) {
                declaration.flags=Read<std::uint32_t>(api.field_get_flags,"field_get_flags failed",field);
                declaration.flags_observed=true;
            }
            fields.push_back(std::move(declaration));
        }
        if (!include_parents) break;
        // Retained table ABI was called with an unused iterator argument.
        current = Read<void*>(api.class_get_parent, "class_get_parent failed", current, static_cast<void*>(nullptr));
    }
    return fields;
}
}
