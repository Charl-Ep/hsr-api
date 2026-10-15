#include <hksr/unity/MeshSkinning.hpp>
#include <Windows.h>
#include <iostream>
#include <stdexcept>

namespace {
using namespace hksr::api;
int managed, native, metadata;
unsigned weight_calls=0, bindpose_calls=0;
bool reject_weights=false, fail_bindposes=false;
void Check(bool ok) { if(!ok) throw std::runtime_error("Mesh skinning contract mismatch"); }
void* __fastcall Pointer(void* object,void* info) { Check(object==&managed && info==&metadata); return &native; }
bool __fastcall Weights(void* object,const FixedBoneWeights* values,std::int32_t count) {
    Check(object==&native && count==1 && values[0].weights[0]==1 && values[0].indices[0]==0); ++weight_calls; return !reject_weights;
}
void __fastcall Bindposes(void* object,const BindposeMatrix* values,std::int32_t count) {
    Check(object==&native && count==1 && values[0][0]==1); ++bindpose_calls;
    if(fail_bindposes) RaiseException(0xE0570001,0,0,nullptr);
}
}
void TestMeshSkinning() {
    MeshSkinning access({{reinterpret_cast<void*>(&Pointer),&metadata},reinterpret_cast<void*>(&Weights),reinterpret_cast<void*>(&Bindposes)});
    FixedBoneWeights weights{{1,0,0,0},{0,0,0,0}}; BindposeMatrix matrix{}; matrix[0]=1;
    Check(access.Write(&managed,&weights,1,&matrix,1).Returned() && weight_calls==1 && bindpose_calls==1);
    reject_weights=true;
    auto rejected=access.Write(&managed,&weights,1,&matrix,1); reject_weights=false;
    Check(rejected.weights.read.returned && !rejected.weights.value && !rejected.bindposes.returned && bindpose_calls==1);
    fail_bindposes=true;
    auto failed=access.Write(&managed,&weights,1,&matrix,1); fail_bindposes=false;
    Check(!failed.Returned() && failed.bindposes.exception==0xE0570001 && failed.native_mesh.object==&native);
    Check(!access.Write(&managed,nullptr,1,&matrix,1).Returned());
    std::cout<<"Mesh skinning tests passed: native ABI and partial-write results.\n";
}
