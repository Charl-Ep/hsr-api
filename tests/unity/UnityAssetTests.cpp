#include <hksr/unity/UnityAssetAccess.hpp>
#include <hksr/metadata/MetadataResolver.hpp>
#include <hksr/unity/OwnedUnityObjects.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <map>
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace {
using namespace hksr::api;
int metadata, mesh, texture, material, source, name, bytes, shader;
unsigned writes=0, applied=0, sampling=0;
bool construct_fault=false, load_result=true, alive=false;
bool shader_type_fault=false;
unsigned dimensions_read=0;
std::map<int,void*> roots;
void Check(bool value) { if(!value) throw std::runtime_error("Unity asset call mismatch"); }
void Info(void* info) { Check(info==&metadata); }
void __fastcall Create(void* object,void* info) { Info(info); Check(object==&mesh); ++writes; }
void __fastcall Index(void* object,MeshIndex index,void* info) { Info(info); Check(object==&mesh && index==MeshIndex::UInt16); ++writes; }
void __fastcall Channel(void* object,MeshAttribute attr,MeshElement format,int dim,const void* data,int length,int start,int count,void* info) {
    Info(info); Check(object==&mesh && attr==MeshAttribute::Color && format==MeshElement::UNorm8 && dim==4 && data==&bytes && length==7 && start==0 && count==7); ++writes;
}
void __fastcall Indices(void* object,int submesh,int topology,MeshIndex format,const void* data,int first,int count,bool bounds,int base,void* info) {
    Info(info); Check(object==&mesh && submesh==2 && topology==0 && format==MeshIndex::UInt16 && data==&bytes && first==6 && count==9 && !bounds && base==0); ++writes;
}
void __fastcall Submeshes(void* object,int count,void* info) { Info(info); Check(object==&mesh && count==3); ++writes; }
void __fastcall Recalculate(void* object,void* info) { Info(info); Check(object==&mesh); ++writes; }
void __fastcall Upload(void* object,bool discard,void* info) { Info(info); Check(object==&mesh && !discard); ++writes; }
int __fastcall Vertices(void* object,void* info) { Info(info); Check(object==&mesh); return 7; }
int __fastcall SubmeshCount(void* object,void* info) { Info(info); Check(object==&mesh); return 3; }
void* __fastcall CloneMesh(void* object,void* info) { Info(info); Check(object==&mesh); return &source; }
int __fastcall ShapeCount(void* object,void* info) { Info(info); Check(object==&mesh); return 2; }
std::vector<void*> elements;
void* __fastcall ArrayCreate(void* type,int count,void* info) { Info(info); Check(type==&name); elements.resize(count); return &elements; }
int __fastcall ArrayLength(void* array,void* info) { Info(info); Check(array==&elements); return static_cast<int>(elements.size()); }
void __fastcall ArraySet(void* array,void* value,int index,void* info) { Info(info); Check(array==&elements); elements.at(index)=value; }
void* __fastcall ArrayGet(void* array,int index,void* info) { Info(info); Check(array==&elements); return elements.at(index); }
void* __fastcall Find(void* root,void* path,void* info) { Info(info); Check(root==&mesh && path==&name); return &source; }
bool __fastcall Weights(void* object,void* info) { Info(info); Check(object==&mesh); return false; }
void __fastcall Construct(void* object,int w,int h,int format,int mips,bool linear,void* native,void* info) {
    Info(info); Check(object==&texture && w==8 && h==16 && format==12 && mips==4 && linear && !native);
    Check(roots.size()==1); alive=true;
    if(construct_fault) RaiseException(0xE0540001,0,0,nullptr);
}
bool __fastcall Load(void* object,const void* data,int size,void* info) { Info(info); Check(object==&texture && data==&bytes && size==4); return load_result; }
void __fastcall Apply(void* object,bool update,bool discard,void* info) { Info(info); Check(object==&texture && !update && discard); ++applied; }
void __fastcall IntSetting(void* object,int,void* info) { Info(info); Check(object==&texture); ++sampling; }
void __fastcall Bias(void* object,float bias,void* info) { Info(info); Check(object==&texture && bias==0.25f); ++sampling; }
int __fastcall TextureDimension(void* object,void* info) { Info(info); Check(object==&texture); return 2; }
void __fastcall Copy(void* object,void* original,void* info) { Info(info); Check(object==&material && original==&source); }
int __fastcall Property(void* text,void* info) { Info(info); Check(text==&name); return -123; }
bool __fastcall Has(void* object,int id,void* info) { Info(info); Check(object==&material && id==-123); return true; }
void __fastcall Bind(void* object,int id,void* value,void* info) { Info(info); Check(object==&material && id==-123 && value==&texture); }
void __fastcall Scale(void* object,int id,Float2 value,void* info) { Info(info); Check(object==&material && id==-123 && value.x==2 && value.y==3); }
void __fastcall Offset(void* object,int id,Float2 value,void* info) { Info(info); Check(object==&material && id==-123 && value.x==0.1f && value.y==0.2f); }
void* __fastcall GetShader(void* object,void* info) { Info(info); Check(object==&material); return &shader; }
int __fastcall PropertyCount(void* object,void* info) { Info(info); Check(object==&shader); return 3; }
int __fastcall NameId(void* object,int index,void* info) { Info(info); Check(object==&shader); return index==0?-123:index; }
int __fastcall PropertyType(void* object,int index,void* info) {
    Info(info); Check(object==&shader);
    if(shader_type_fault && index==1) RaiseException(0xE0540002,0,0,nullptr);
    return index==1?2:4;
}
int __fastcall Dimension(void* object,int index,void* info) { Info(info); Check(object==&shader && index!=1); ++dimensions_read; return index==0?2:4; }
int __fastcall Keep(void* object,int zero,int kind,void*) { Check(!zero && kind==2); roots[1]=object; return 1; }
void* __fastcall Target(int id,void*) { return roots.at(id); }
void __fastcall Free(int id,void*) { roots.erase(id); }
bool __fastcall Alive(void* object,void*) { Check(object==&texture); return alive; }
void __fastcall Destroy(void* object,void*) { Check(object==&texture); alive=false; }
template<class T> Method M(T function) { return {reinterpret_cast<void*>(function),&metadata}; }
std::vector<std::string> Split(const std::string& s,char separator) {
    std::vector<std::string> result; std::istringstream in(s); std::string part;
    while(std::getline(in,part,separator)) result.push_back(part);
    return result;
}
}
void TestUnityAssets(const char* fixture) {
    // Actual retained signatures (including competing overloads); only the
    // pointers are replaced. This still does not execute a target-game object.
    const std::map<std::string,Method> calls{
        {"Mesh.Internal_Create",M(Create)}, {"Mesh.set_indexFormat",M(Index)},
        {"Mesh.SetSizedNativeArrayForChannel",M(Channel)}, {"Mesh.SetIndicesNativeArrayImpl",M(Indices)},
        {"Mesh.set_subMeshCount",M(Submeshes)}, {"Mesh.RecalculateBounds",M(Recalculate)},
        {"Mesh.UploadMeshData",M(Upload)}, {"Mesh.get_vertexCount",M(Vertices)},
        {"Mesh.get_subMeshCount",M(SubmeshCount)}, {"Mesh.HasBoneWeights",M(Weights)},
        {"Mesh.get_blendShapeCount",M(ShapeCount)}, {"Object.Instantiate",M(CloneMesh)},
        {"Array.CreateInstance",M(ArrayCreate)}, {"Array.get_Length",M(ArrayLength)},
        {"Array.SetValue",M(ArraySet)}, {"Array.GetValue",M(ArrayGet)}, {"Transform.Find",M(Find)},
        {"Texture2D..ctor",M(Construct)}, {"Texture2D.LoadRawTextureDataImpl",M(Load)}, {"Texture2D.ApplyImpl",M(Apply)},
        {"Texture.set_filterMode",M(IntSetting)}, {"Texture.set_anisoLevel",M(IntSetting)}, {"Texture.set_mipMapBias",M(Bias)},
        {"Texture.set_wrapModeU",M(IntSetting)}, {"Texture.set_wrapModeV",M(IntSetting)}, {"Texture.set_wrapModeW",M(IntSetting)},
        {"Texture.get_dimension",M(TextureDimension)},
        {"Material.CreateWithMaterial",M(Copy)}, {"Shader.PropertyToID",M(Property)}, {"Material.HasProperty",M(Has)},
        {"Material.SetTexture",M(Bind)}, {"Material.SetTextureScale",M(Scale)}, {"Material.SetTextureOffset",M(Offset)},
        {"Material.get_shader",M(GetShader)}, {"Shader.GetPropertyCount",M(PropertyCount)},
        {"Shader.GetPropertyNameId",M(NameId)}, {"Shader.GetPropertyType",M(PropertyType)}, {"Shader.GetPropertyTextureDimension",M(Dimension)}
    };
    std::map<std::string,DeclaredType> types;
    std::ifstream in(fixture); Check(in.good()); std::string line;
    while(std::getline(in,line)) {
        const auto row=Split(line,'\t'); Check(row.size()==5);
        const auto dot=row[0].rfind('.'); Check(dot!=std::string::npos);
        const auto type=row[0].substr(dot+1);
        auto& d=types[row[0]]; d.name_space=row[0].substr(0,dot); d.name=type;
        const auto found=calls.find(type+"."+row[1]);
        if(found!=calls.end()) d.methods.push_back({row[1],row[2],Split(row[3],';'),(std::stoul(row[4])&0x10)!=0,found->second});
    }
    std::vector<DeclaredType> declarations; for(auto& pair:types) declarations.push_back(std::move(pair.second));
    MetadataResolver resolver(declarations);
    const ManagedArrays arrays(ResolveArrayMethods(resolver));
    const auto array=arrays.Create(&name,2); Check(array.call.returned && array.object==&elements);
    Check(arrays.Length(array.object).value==2 && arrays.Set(array.object,&mesh,1).returned && arrays.Get(array.object,1).object==&mesh);
    Check(HierarchyAccess(ResolveHierarchyMethods(resolver)).Find(&mesh,&name).object==&source);
    const MeshAccess ma(ResolveMeshMethods(resolver));
    const MeshTemplateAccess templates(ResolveMeshTemplateMethods(resolver));
    Check(templates.Clone(&mesh).object==&source && templates.VertexCount(&mesh).value==7 && templates.ShapeCount(&mesh).value==2);
    Check(ma.Initialize(&mesh).returned && ma.IndexFormat(&mesh,MeshIndex::UInt16).returned);
    Check(ma.Channel(&mesh,MeshAttribute::Color,MeshElement::UNorm8,4,&bytes,7).returned);
    Check(ma.Submeshes(&mesh,3).returned && ma.Triangles(&mesh,2,MeshIndex::UInt16,&bytes,6,9).returned);
    Check(ma.RecalculateBounds(&mesh).returned && ma.Upload(&mesh,false).returned && writes==7);
    const auto counts=ma.Counts(&mesh); Check(counts.vertices.read.returned && counts.vertices.value==7 && counts.submeshes.value==3 && counts.has_weights.read.returned && !counts.has_weights.value);
    const TextureAccess ta(ResolveTextureMethods(resolver));
    OwnedUnityObjects owned({M(Alive),M(Destroy)},{M(Keep),M(Target),M(Free),{}, {}});
    const auto id=owned.Adopt(&texture); construct_fault=true;
    const auto ctor=ta.Initialize(&texture,{8,16,12,4,true}); Check(!ctor.returned && ctor.exception==0xE0540001 && roots.size()==1 && alive);
    Check(!owned.Close(id).complete && !alive && owned.Close(id).complete && roots.empty()); owned.Forget(id);
    load_result=false; const auto rejected=ta.LoadRaw(&texture,&bytes,4); Check(rejected.read.returned && !rejected.value && applied==0);
    load_result=true; Check(ta.LoadRaw(&texture,&bytes,4).value);
    Check(ta.Sampling(&texture,{1,2,0,1,2,0.25f}).Returned() && sampling==6);
    Check(ta.Apply(&texture,false,true).returned && applied==1);
    Check(ta.Dimension(&texture).read.returned && ta.Dimension(&texture).value==2);
    const MaterialAccess material_api(ResolveMaterialMethods(resolver));
    Check(material_api.InitializeCopy(&material,&source).returned);
    Check(material_api.PropertyId(&name).value==-123 && material_api.HasProperty(&material,-123).value);
    Check(material_api.Texture(&material,-123,&texture).returned && material_api.Scale(&material,-123,{2,3}).returned && material_api.Offset(&material,-123,{0.1f,0.2f}).returned);
    const ShaderAccess shader_api(ResolveShaderMethods(resolver));
    const auto properties=shader_api.Inspect(&material);
    Check(properties.complete && properties.properties.size()==3 && dimensions_read==2 && properties.properties[0].id.value==-123 && properties.properties[1].type.value==2 && !properties.properties[1].texture_dimension.read.returned);
    shader_type_fault=true; const auto partial=shader_api.Inspect(&material); shader_type_fault=false;
    Check(!partial.complete && partial.properties.size()==3 && partial.properties[1].type.read.exception==0xE0540002 && partial.properties[2].texture_dimension.read.returned);
    declarations.erase(std::remove_if(declarations.begin(),declarations.end(),[](const DeclaredType& d){ return d.name=="Texture2D"; }),declarations.end());
    MetadataResolver without_textures(declarations);
    Check(ResolveMeshMethods(without_textures).create.code && ResolveMaterialMethods(without_textures).copy.code);
    auto partial_methods=ResolveTextureMethods(without_textures);
    Check(partial_methods.binding_errors.size()==3 && !partial_methods.construct.code && partial_methods.filter.code);
    for(auto& d:declarations) if(d.name=="Mesh") d.methods.erase(std::remove_if(d.methods.begin(),d.methods.end(),[](const DeclaredMethod& m){ return m.name=="HasBoneWeights"; }),d.methods.end());
    MetadataResolver without_readback(declarations);
    auto partial_mesh=ResolveMeshMethods(without_readback);
    Check(partial_mesh.binding_errors.size()==1 && !partial_mesh.has_weights.code && MeshAccess(partial_mesh).Initialize(&mesh).returned);
    std::cout<<"Unity asset calls passed: observed overloads, ABI, independent groups and partial constructor cleanup.\n";
}
