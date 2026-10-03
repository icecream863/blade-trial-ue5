#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "SCLCombatInterface.generated.h"

class USCLCombatComponent;

// 供蓝图识别战斗对象的 Unreal 接口反射类型。
UINTERFACE(BlueprintType)
class SOULCOMBATLAB_API USCLCombatInterface : public UInterface
{
	GENERATED_BODY()
};

// 战斗接口：让调用方从角色取得战斗组件，而不必依赖具体玩家或敌人类型。
class SOULCOMBATLAB_API ISCLCombatInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat")
	USCLCombatComponent* GetCombatComponent() const;
};

