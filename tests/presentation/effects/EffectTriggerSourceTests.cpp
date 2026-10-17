#include <hksr/presentation/effects/EffectTriggerSource.hpp>
#include <map>
#include <cstddef>
#include <stdexcept>

namespace {
using namespace hksr::api;
struct Config {int padding[7];void* path=nullptr;};
std::map<int,void*> roots;int next=0;
void Check(bool value){if(!value)throw std::runtime_error("effect trigger source test failed");}
int __fastcall Keep(void* object,int,int,void*){roots[++next]=object;return next;}
void* __fastcall Target(int id,void*){return roots.at(id);}
void __fastcall Free(int id,void*){roots.erase(id);}
int __fastcall Length(std::wstring* s,void*){return static_cast<int>(s->size());}
wchar_t __fastcall Character(std::wstring* s,int i,void*){return s->at(i);}
template<class T>Method M(T entry){return {reinterpret_cast<void*>(entry),nullptr};}
}
void TestEffectTriggerSource() {
    ComponentConfigMethods methods;methods.string_length=M(Length);methods.string_character=M(Character);
    const ReferenceMethods refs{M(Keep),M(Target),M(Free),{}, {}};
    EffectTriggerSource reader({{"RPG.GameCore","TriggerEffect","EffectPath","System.String",offsetof(Config,path),6,true}},refs,ComponentConfig(methods));
    std::wstring text=L"Effects/UI_01.prefab";Config config{};config.path=&text;
    EffectTriggerExecution trigger{1,nullptr,&config,nullptr};
    const auto path=reader.Path(trigger);Check(path.read.returned && path.utf8=="Effects/UI_01.prefab" && roots.empty());
    config.path=nullptr;const auto empty=reader.Path(trigger);Check(empty.read.returned && empty.utf8.empty() && roots.empty());
    trigger.serial=0;Check(!reader.Path(trigger).read.returned && roots.empty());
}
