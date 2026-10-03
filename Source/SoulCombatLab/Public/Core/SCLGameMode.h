#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "SCLGameMode.generated.h"

class ASCLPlayerCharacter;

// 选择玩家 Pawn、Controller 和 HUD 的默认类，供首次生成与 Demo 重生使用。
UCLASS(Blueprintable)
class SOULCOMBATLAB_API ASCLGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASCLGameMode();
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Classes")
	TSubclassOf<ASCLPlayerCharacter> PlayerCharacterClass;

	virtual void StartPlay() override;
};
