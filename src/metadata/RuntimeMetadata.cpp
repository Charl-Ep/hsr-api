#include <hksr/metadata/RuntimeMetadata.hpp>
#include "MetadataConnection.hpp"
#include "MetadataTypeIndex.hpp"
#include "MetadataCache.hpp"
#include <hksr/native/MetadataProvider.hpp>
#include <map>
#include <stdexcept>
#include <utility>

namespace hksr::sdk_v1 {
struct RuntimeMetadata::Storage {
    RuntimeVersion version;
    native::NativeMetadataReader reader;
    std::vector<native::NativeType> types;
    detail::MetadataTypeIndex names;
    detail::MetadataCache<void*,native::NativeType> descriptions;
    detail::MetadataCache<void*,DeclaredType> methods;
    detail::MetadataCache<std::pair<void*,bool>,std::vector<DeclaredField>> fields;
    std::vector<std::string> unavailable;
    native::ObjectFunctions objects;
    Storage(RuntimeVersion selected,native::MetadataConnection connection) : version(selected),reader(connection), types(reader.ReadTypes()), unavailable(std::move(connection.unavailable_operations)), objects(connection.objects) {
        for(std::size_t i=0;i<types.size();++i) {
            names.Add(types[i].name_space,types[i].name,i);
            descriptions.Seed(types[i].type,types[i]);
        }
    }
    native::NativeType Describe(void* klass) {
        return descriptions.Get(klass,[&]{return reader.DescribeType(klass);});
    }
    const native::NativeType& Find(const char* space, const char* name) const {
        return types.at(names.Find(space,name));
    }
    DeclaredType Get(const char* space, const char* name) {
        return Get(Find(space, name));
    }
    DeclaredType Get(const native::NativeType& match) {
        return methods.Get(match.type,[&]{return reader.ReadMethods(match);});
    }
};
RuntimeMetadata::RuntimeMetadata(Storage* storage) : storage_(storage) {}
RuntimeMetadata::~RuntimeMetadata() { delete storage_; }
std::unique_ptr<RuntimeMetadata> RuntimeMetadata::Open(RuntimeVersion version) {
    return native::MetadataProvider::Open(version,detail::ConnectRuntimeMetadata(version));
}
std::unique_ptr<RuntimeMetadata> native::MetadataProvider::Open(RuntimeVersion version,MetadataConnection connection) {
    auto storage = std::make_unique<RuntimeMetadata::Storage>(version,std::move(connection));
    auto result = std::unique_ptr<RuntimeMetadata>(new RuntimeMetadata(storage.get()));
    storage.release();
    return result;
}
Method RuntimeMetadata::Resolve(const char* space, const char* type, const char* name,
    const char* result, std::initializer_list<const char*> parameters, MethodKind kind) {
    MetadataResolver resolver({storage_->Get(space, type)});
    return resolver.Resolve(space, type, name, result, parameters, kind);
}
DeclaredType RuntimeMetadata::ReadType(const char* space, const char* name) { return storage_->Get(space, name); }
std::vector<DeclaredField> RuntimeMetadata::ReadFields(const char* space, const char* name, bool include_parents) {
    return ReadFields(FindType(space,name),include_parents);
}
std::size_t RuntimeMetadata::TypeCount() const { return storage_->types.size(); }
RuntimeCodeEntries RuntimeMetadata::ReadCodeEntries(std::size_t index) const {
    return storage_->reader.ReadCodeEntries(storage_->types.at(index));
}
DeclaredType RuntimeMetadata::ReadTypeAt(std::size_t index) {
    return storage_->Get(storage_->types.at(index));
}
RuntimeType RuntimeMetadata::TypeAt(std::size_t index) {return {storage_->types.at(index).type,this};}
RuntimeType RuntimeMetadata::ParentType(RuntimeType type) {
    const auto parent=storage_->reader.ParentType(storage_->Describe(CheckedClass(type)));
    return parent.type?RuntimeType{parent.type,this}:RuntimeType{};
}
RuntimeTypeName RuntimeMetadata::TypeName(RuntimeType type) {
    const auto declaration=storage_->Describe(CheckedClass(type));
    return {declaration.name_space,declaration.name};
}
bool RuntimeMetadata::TypeDerivesFrom(RuntimeType type,RuntimeType base) {
    return storage_->reader.DerivesFrom(CheckedClass(type),CheckedClass(base));
}
RuntimeFields RuntimeMetadata::ObserveFields(RuntimeType type) {
    const auto observed=storage_->reader.ObserveFields(storage_->Describe(CheckedClass(type)));
    RuntimeFields result;result.iteration_error=observed.iteration_error;
    for(const auto& row:observed.fields)
        result.fields.push_back({RuntimeField{row.metadata,type,this},row.declaration,row.error});
    return result;
}
RuntimeType RuntimeMetadata::DeclaringType(RuntimeField field) const {
    if(field.provider_!=this || !field.metadata_)throw std::invalid_argument("runtime field belongs to another provider or is empty");
    CheckedClass(field.owner_);return field.owner_;
}
RuntimeType RuntimeMetadata::FieldType(RuntimeField field) {
    DeclaringType(field);return {storage_->reader.FieldType(field.metadata_).type,this};
}
std::vector<RuntimeTypeName> RuntimeMetadata::TypeNames() const {
    std::vector<RuntimeTypeName> result;result.reserve(storage_->types.size());
    for(const auto& type:storage_->types)result.push_back({type.name_space,type.name});
    return result;
}
RuntimeVersion RuntimeMetadata::Version() const { return storage_->version; }
std::vector<std::string> RuntimeMetadata::UnavailableOperations() const { return storage_->unavailable; }
ObjectResult RuntimeMetadata::NewStringUtf8(const char* text) { return native::NewString(storage_->objects, text); }
ObjectResult RuntimeMetadata::AllocateObject(const char* space, const char* name) {
    return native::AllocateObject(storage_->objects, storage_->Find(space, name).type);
}
ObjectResult RuntimeMetadata::ManagedType(const char* space, const char* name) {
    return native::TypeObject(storage_->objects, storage_->Find(space, name).type);
}
void* RuntimeMetadata::CheckedClass(RuntimeType type) const {
    if (type.provider_ != this || !type.klass_) throw std::invalid_argument("runtime type belongs to another provider or is empty");
    return type.klass_;
}
RuntimeType RuntimeMetadata::FindType(const char* space, const char* name) {
    return {storage_->Find(space, name).type, this};
}
RuntimeType RuntimeMetadata::ParameterType(Method method, unsigned index) {
    return {storage_->reader.ParameterType(method, index).type, this};
}
RuntimeType RuntimeMetadata::ReturnType(Method method) {
    return {storage_->reader.ReturnType(method).type,this};
}
RuntimeType RuntimeMetadata::ObjectType(void* object) {
    // Qualified IL2CPP object-header contract, not a
    // portable Unity ABI. Names/declarations are subsequently read by the API.
    void* klass=nullptr;
    if(!object || !native::CopyMemoryChecked(&klass,object,sizeof(klass)) || !klass)
        throw std::runtime_error("managed object class unavailable");
    storage_->Describe(klass);
    return {klass,this};
}
DeclaredType RuntimeMetadata::ReadObjectType(void* object) { return ReadType(ObjectType(object)); }
RuntimeTypeName RuntimeMetadata::ReadObjectTypeName(void* object) {
    const auto& type=storage_->Describe(CheckedClass(ObjectType(object)));
    return {type.name_space,type.name};
}
std::vector<DeclaredField> RuntimeMetadata::ReadObjectFields(void* object) { return ReadFields(ObjectType(object)); }
bool RuntimeMetadata::TypeDerivesFrom(const char* space,const char* name,const char* base_space,const char* base_name) {
    return storage_->reader.DerivesFrom(storage_->Find(space,name).type,storage_->Find(base_space,base_name).type);
}
DeclaredType RuntimeMetadata::ReadType(RuntimeType type) {
    return storage_->Get(storage_->Describe(CheckedClass(type)));
}
std::vector<DeclaredField> RuntimeMetadata::ReadFields(RuntimeType type, bool include_parents) {
    const auto key=std::make_pair(CheckedClass(type),include_parents);
    return storage_->fields.Get(key,[&]{return storage_->reader.ReadFields(storage_->Describe(key.first),include_parents);});
}
Method RuntimeMetadata::Resolve(RuntimeType type, const char* name, const char* result,
    std::initializer_list<const char*> parameters, MethodKind kind) {
    const auto declaration = ReadType(type);
    MetadataResolver resolver({declaration});
    return resolver.Resolve(declaration.name_space.c_str(), declaration.name.c_str(), name, result, parameters, kind);
}
ObjectResult RuntimeMetadata::AllocateObject(RuntimeType type) { return native::AllocateObject(storage_->objects, CheckedClass(type)); }
ObjectResult RuntimeMetadata::ManagedType(RuntimeType type) { return native::TypeObject(storage_->objects, CheckedClass(type)); }
}
