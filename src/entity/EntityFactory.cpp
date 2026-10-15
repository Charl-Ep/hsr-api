#include <hksr/entity/EntityFactory.hpp>
#include <hksr/native/Invocation.hpp>

namespace hksr::sdk_v1 {
CreateEntityResult EntityFactory::CreateMapNpc(EntityManager* manager,MapNpcDefinition* definition,
    NpcDataRow* row,EntityCallback* callback,bool option) const {
    CreateEntityResult result;
    result.call=native::Call(create_map_,&result.entity,manager,definition,row,callback,option);
    return result;
}
}
