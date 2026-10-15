#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/native/ObjectAccess.hpp>
#include <cstdint>
#include <functional>
#include <vector>

namespace hksr::sdk_v1::native {
// Native table functions have NO trailing MethodInfo argument. These are not
// interchangeable with the managed invocation wrappers in Invocation.hpp.
struct MetadataFunctions {
    void* domain_get;
    void* domain_get_assemblies;
    void* assembly_get_image;
    void* image_class_count;
    void* image_get_class;
    void* class_get_name;
    void* class_get_namespace;
    void* class_get_methods;
    void* method_get_name;
    void* method_return_type;
    void* method_param_count;
    void* method_get_param;
    void* type_get_name;
    void* release_type_name;
    void* class_get_parent;
    void* class_get_fields;
    void* field_get_name;
    void* field_get_type;
    void* field_get_offset;
    void* class_from_type = nullptr;
    void* field_get_flags = nullptr;
};
struct MetadataLayout {
    std::size_t method_code_offset;
    std::size_t method_flags_offset;
};
struct NativeType {
    std::string name_space, name;
    void* type;
    void* image;
};
// Discovery data, deliberately not DeclaredMethod/MethodResolver input.
// No staticness or callable address is inferred from an unqualified layout.
struct MethodObservation {
    void* metadata=nullptr;
    std::string name, result;
    std::vector<std::string> parameters;
    std::string error;
};
struct MethodObservations {
    std::vector<MethodObservation> methods;
    std::string iteration_error;
};
struct TypeObservations {
    std::vector<NativeType> types;
    std::vector<std::string> errors;
};
struct FieldObservation {
    void* metadata=nullptr;
    DeclaredField declaration;
    std::string error;
};
struct FieldObservations {
    std::vector<FieldObservation> fields;
    std::string iteration_error;
};
struct MetadataConnection {
    MetadataFunctions functions;
    MetadataLayout layout;
    std::function<bool(void*)> executable;
    std::vector<std::string> unavailable_operations;
    ObjectFunctions objects;
};

// Native adapter/diagnostic support. Requires caller-qualified tables and layout;
// observations do not qualify a method for execution. No runtime auto-discovery.
class NativeMetadataReader {
public:
    explicit NativeMetadataReader(MetadataConnection connection);
    TypeObservations ObserveTypes() const;
    std::vector<NativeType> ReadTypes() const;
    MethodObservations ObserveMethods(const NativeType& type) const;
    DeclaredType ReadMethods(const NativeType& type) const;
    RuntimeCodeEntries ReadCodeEntries(const NativeType& type) const;
    FieldObservations ObserveFields(const NativeType& type) const;
    NativeType ParentType(const NativeType& type) const;
    NativeType FieldType(void* field) const;
    std::vector<DeclaredField> ReadFields(const NativeType& type, bool include_parents) const;
    NativeType ParameterType(Method method, unsigned index) const;
    NativeType ReturnType(Method method) const;
    NativeType DescribeType(void* klass) const;
    bool DerivesFrom(void* klass,void* base) const;
private:
    std::string TypeName(void* type) const;
    MetadataConnection connection_;
};
bool CopyMemoryChecked(void* destination, const void* source, std::size_t size);
std::string CopyNativeString(const char* source);
}
