#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "SCLEnemyArchetypeData.generated.h"

class UMaterialInterface;

// 敌人类型的数据资产基类：配置战斗数值、外观与攻击差异。
UCLASS(Abstract, BlueprintType)
class SOULCOMBATLAB_API USCLEnemyArchetypeData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	FName GetArchetypeName() const { return ArchetypeName; }
	float GetMoveSpeed() const { return MoveSpeed; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetMaxPoise() const { return MaxPoise; }
	float GetDamageScale() const { return DamageScale; }
	float GetAttackRecoveryDuration() const { return AttackRecoveryDuration; }
	int32 GetAttacksPerBurst() const { return AttacksPerBurst; }
	bool AreAttacksParryable() const { return bAttacksParryable; }
	float GetWeaponVisualScale() const { return WeaponVisualScale; }
	const TArray<TObjectPtr<UMaterialInterface>>& GetBodyMaterials() const { return BodyMaterials; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FName ArchetypeName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float MoveSpeed{500.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attributes", meta = (ClampMin = "1.0"))
	float MaxHealth{100.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attributes", meta = (ClampMin = "0.0"))
	float MaxPoise{50.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack", meta = (ClampMin = "0.0"))
	float DamageScale{1.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack", meta = (ClampMin = "0.0", Units = "s"))
	float AttackRecoveryDuration{0.75F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack", meta = (ClampMin = "1"))
	int32 AttacksPerBurst{2};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Attack")
	bool bAttacksParryable{true};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation", meta = (ClampMin = "0.1"))
	float WeaponVisualScale{1.0F};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Presentation")
	TArray<TObjectPtr<UMaterialInterface>> BodyMaterials;
};

// 剑兵配置资产类型，提供剑兵默认参数。
UCLASS()
class SOULCOMBATLAB_API USCLSwordEnemyArchetypeData final : public USCLEnemyArchetypeData
{
	GENERATED_BODY()

public:
	USCLSwordEnemyArchetypeData();
};

// 重兵配置资产类型，提供重兵默认参数。
UCLASS()
class SOULCOMBATLAB_API USCLHeavyEnemyArchetypeData final : public USCLEnemyArchetypeData
{
	GENERATED_BODY()

public:
	USCLHeavyEnemyArchetypeData();
};

// Boss 配置资产类型，提供 Boss 默认参数。
UCLASS()
class SOULCOMBATLAB_API USCLBossArchetypeData final : public USCLEnemyArchetypeData
{
	GENERATED_BODY()

public:
	USCLBossArchetypeData();
};
