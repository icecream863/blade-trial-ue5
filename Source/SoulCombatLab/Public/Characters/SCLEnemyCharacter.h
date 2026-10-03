#pragma once

#include "Characters/SCLCharacterBase.h"
#include "CoreMinimal.h"

#include "SCLEnemyCharacter.generated.h"

class USCLEnemyArchetypeData;
class UWidgetComponent;

// 敌人角色基类：读取 Archetype 配置属性与外观，并接入 AI 和头顶血条。
UCLASS()
class SOULCOMBATLAB_API ASCLEnemyCharacter : public ASCLCharacterBase
{
	GENERATED_BODY()

public:
	ASCLEnemyCharacter();

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	const USCLEnemyArchetypeData* GetArchetypeData() const;
	float GetAttackRecoveryDuration() const;
	int32 GetAttacksPerBurst() const;
	virtual float GetCombatEnterDistance() const;
	virtual float GetCombatExitDistance() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	bool bShowOverheadHealthBar{true};
	UPROPERTY(VisibleAnywhere, Category = "Enemy|UI")
	TObjectPtr<UWidgetComponent> OverheadHealthBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Archetype")
	TSubclassOf<USCLEnemyArchetypeData> ArchetypeDataClass;
};

// 剑兵预设类，选择剑兵 Archetype；通用战斗行为仍由敌人基类和 AI 执行。
UCLASS()
class SOULCOMBATLAB_API ASCLSwordEnemyCharacter final : public ASCLEnemyCharacter
{
	GENERATED_BODY()

public:
	ASCLSwordEnemyCharacter();
};

// 重兵预设类，选择重兵 Archetype；通用战斗行为仍由敌人基类和 AI 执行。
UCLASS()
class SOULCOMBATLAB_API ASCLHeavyEnemyCharacter final : public ASCLEnemyCharacter
{
	GENERATED_BODY()

public:
	ASCLHeavyEnemyCharacter();
};
