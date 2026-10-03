#pragma once

#include "CoreMinimal.h"
#include "Characters/SCLCharacterBase.h"

#include "SCLTrainingDummy.generated.h"

// 训练目标角色：复用角色基类的属性和受击入口，供战斗场景与测试使用。
UCLASS()
class SOULCOMBATLAB_API ASCLTrainingDummy : public ASCLCharacterBase
{
	GENERATED_BODY()

public:
	ASCLTrainingDummy();
};

